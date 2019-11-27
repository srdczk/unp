#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/epoll.h>
// 线程的pid
#define BUFSIZE 1024
#define PROCESS_LIMIT 65536
#define CHILD_EPOLL_SIZE 5
#define EPOLL_SIZE 2000
#define USER_LIMIT 5
#define PORT 9999
#define NAME "/my_shm"
// 最大的 user 数
struct client_data {
    struct sockaddr_in addr;
    int cfd;
    // client 私有管道
    int pipefd[2];
    pid_t pid;
};
int epfd, sfd, shfd, sig_pipefd[2], user_count = 0;
// 起始的数据段
char *start = 0;
struct client_data *users = 0;
int *process = 0;
int child_stop = 0;
int set_nonblocking(int fd) {
    int option = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, option | O_NONBLOCK);
    return option;
}

void error_handling(const char *msg) {
    perror(msg);
    exit(-1);
}

void sig_handler(int sig) {
    int old_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char *)&msg, 1, 0);
    errno = old_errno;
}

void add_sig(int sig, void (*handler)(int), int restart) {
    //
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    if (sigaction(sig, &sa, NULL) == -1) error_handling("sigaction error");
}

void del_resources() {
    // 删除资源
    close(sig_pipefd[0]);
    close(sig_pipefd[1]);
    close(sfd);
    close(epfd);
    shm_unlink(NAME);
    free(users);
    free(process);
}

void addfd(int ep, int fd) {
    struct epoll_event event;
    memset(&event, '\0', sizeof(event));
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(ep, EPOLL_CTL_ADD, fd, &event) == -1) error_handling("epoll add error");
    // 设置非阻塞
    set_nonblocking(fd);
}
void child_term_handler(int sig) {
    child_stop = 1;
}
int child_run(int idx, struct client_data *users, char *start) {
    // 初始化 child_epoll
    int child_epfd = epoll_create(CHILD_EPOLL_SIZE);
    if (child_epfd == -1) error_handling("epoll create error");
    struct client_data data = users[idx];
    int cfd = data.cfd;
    int pipefd = data.pipefd[1];
    struct epoll_event events[CHILD_EPOLL_SIZE];
    addfd(child_epfd, cfd);
    addfd(child_epfd, pipefd);
    add_sig(SIGTERM, child_term_handler, 0);
    while (!child_stop) {
        int num = epoll_wait(child_epfd, events, CHILD_EPOLL_SIZE, -1);
        if (num < 0 && errno != EAGAIN) {
            printf("epoll listen fail\n");
            break;
        }
        printf("num\n");
        for (int i = 0; i < num; ++i) {
            struct epoll_event *ptr = &events[i];
            if (!(ptr->events & EPOLLIN)) continue;
            if (ptr->data.fd == cfd) {
                printf("cfd !\n");
                // 若果有东西开始进来
                memset(start + idx * BUFSIZE, '\0', BUFSIZE);
                int ret = recv(cfd, start + idx * BUFSIZE, BUFSIZE, 0);
                printf("%s\n", start + idx * BUFSIZE);
                if (ret < 0) {
                    if (errno != EAGAIN) child_stop = 1;
                } else if (!ret) child_stop = 1;
                // 接收成功后, 直接向主进程写入id号
                else {
                    printf("\n");
                    // 向管道写入 !
                    send(pipefd, (char *)&idx, sizeof(idx), 0);
                }
            } else if (ptr->data.fd == pipefd) {
                // 如果管道有东西进来
                // 有人发送了消息
                printf("pipe\n");
                int client = 0;
                int ret = recv(pipefd, (char *)&client, sizeof(client), 0);
                if (ret < 0) {
                    if (errno != EAGAIN) child_stop = 1;
                } else if (!ret) child_stop = 1;
                // 直接向客户端写入
                else send(cfd, start + client * BUFSIZE, BUFSIZE, 0);
            }
        }
    }
    close(cfd);
    close(pipefd);
    close(child_epfd);
    // 子进程 <-->
    return 0;
}

int main() {
    struct epoll_event events[EPOLL_SIZE];
    struct sockaddr_in addr;

    // 申请 sig
    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if ((sfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) error_handling("socket error");
    if (bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) error_handling("bind error");
    if (listen(sfd, 64) == -1) error_handling("listen error");

    users = (struct client_data*)malloc(sizeof(struct client_data) * (USER_LIMIT + 1));
    process = (int*)malloc(sizeof(int) * PROCESS_LIMIT);
    for (int i = 0; i < PROCESS_LIMIT; ++i) {
        process[i] = -1;
    }

    if ((epfd = epoll_create(EPOLL_SIZE)) == -1) error_handling("epoll create error");
    addfd(epfd, sfd);
    if (socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd) == -1) error_handling("socket pair error");
    set_nonblocking(sig_pipefd[1]);
    addfd(epfd, sig_pipefd[0]);

    add_sig(SIGCHLD, sig_handler, 1);
    add_sig(SIGTERM, sig_handler, 1);
    add_sig(SIGINT, sig_handler, 1);
    add_sig(SIGPIPE, SIG_IGN, 1);
    int server_stop = 0, terminate = 0;
    // 创建共享内存
    shfd = shm_open(NAME, O_CREAT | O_RDWR, 0666);
    if (shfd == -1) error_handling("shm_open error");
    if (ftruncate(shfd, USER_LIMIT * BUFSIZE) == -1) error_handling("ftruncate error");
    start = (char *)mmap(NULL, USER_LIMIT * BUFSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shfd, 0);
    if (start == MAP_FAILED) error_handling("mmap error");
    close(shfd);

    while (!server_stop) {
        int num = epoll_wait(epfd, events, EPOLL_SIZE, -1);
        if (num < 0 && errno != EAGAIN) {
            printf("epoll wait fail\n");
            break;
        }
        for (int i = 0; i < num; ++i) {
            struct epoll_event *ptr = &events[i];
            if (!(ptr->events & EPOLLIN)) continue;
            if (ptr->data.fd == sfd) {
                // 有新的连接
                // 创建新的连接
                printf("A new client\n");
                struct sockaddr_in caddr;
                socklen_t size = sizeof(caddr);
                int cfd = accept(sfd, (struct sockaddr*)&caddr, &size);
                if (cfd == 1) {
                    printf("accept error\n");
                    continue;
                }
                if (user_count >= USER_LIMIT) {
                    const char msg[] = "sorry I'm tired\n";
                    send(cfd, msg, sizeof(msg), 0);
                    close(cfd);
                    continue;
                }
                users[user_count].addr = caddr;
                users[user_count].cfd = cfd;
                int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, users[user_count].pipefd);
                if (ret == -1) continue;
                pid_t pid = fork();
                if (!pid) {
                    // 在子进程中关闭所有的
                    close(epfd);
                    close(sfd);
                    close(sig_pipefd[0]);
                    close(sig_pipefd[1]);
                    close(users[user_count].pipefd[0]);
                    child_run(user_count, users, start);
                    munmap((void *)start, USER_LIMIT * BUFSIZE);
                    exit(0);
                } else if (pid > 0) {
                    close(cfd);
                    close(users[user_count].pipefd[1]);
                    addfd(epfd, users[user_count].pipefd[0]);
                    users[user_count].pid = pid;
                    process[pid] = user_count++;
                } else {
                    close(cfd);
                    continue;
                }
            } else if (ptr->data.fd == sig_pipefd[0]) {
                // 如果是信号处理
                char sig[BUFSIZE];
                int len = recv(sig_pipefd[0], sig, BUFSIZE, 0);
                if (len <= 0) continue;
                for (int i = 0; i < len; ++i) {
                    switch (sig[i]) {
                        case SIGCHLD: {
                            // 子进程退出
                            pid_t pid;
                            int stat;
                            while (pid = waitpid(-1, &stat, WNOHANG) > 0) {
                                // 用子进程的pid获取
                                int del_user = process[pid];
                                process[pid] = -1;
                                if (del_user < 0 || del_user > USER_LIMIT) continue;
                                epoll_ctl(users[del_user].cfd, EPOLL_CTL_DEL, users[del_user].cfd, NULL);
                                close(users[del_user].pipefd[0]);
                                users[del_user] = users[--user_count];
                                process[users[del_user].pid] = del_user;
                            }
                            if (terminate && !user_count) {
                                server_stop = 1;
                            }
                            break;
                        }
                        case SIGTERM:
                        case SIGINT:
                        {
                            //结束服务器程序
                            for (int j = 0; j < user_count; ++j) {
                                kill(users[j].pid, SIGTERM);
                            }
                            terminate = 1;
                            break;
                        }
                        default:break;
                    }
                }
            } else {
                // 有人写了数据进来
                // 可以知道, 监听的永远都是 - pipe[1]
                int client = 0;
                int ret = recv(ptr->data.fd, (char *)&client, sizeof(client), 0);
                if (ret < 0) continue;
                for (int j = 0; j < user_count; ++j) {
                    // 向他们的管道写入
                    if (j == client) continue;
                    send(users[j].pipefd[0], (char *)&client, sizeof(client), 0);
                }
            }
        }
    }
    del_resources();
    return 0;
}
