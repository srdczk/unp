#include <iostream>
#include <signal.h>
#include <string.h>
#include <list>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
// 工作线程数量
#define WORKER_THREAD_NUM 5
#define EPOLL_SIZE 2000
#define IP "127.0.0.1"
#define PORT 9999
static int epfd = 0;
bool stop = 0;
int sfd = 0;
// 接受请求并且轮询的线程
pthread_t accept_thread = 0;

pthread_t threads[WORKER_THREAD_NUM] = {0};
pthread_cond_t accept_cond;
// 互斥量
pthread_mutex_t accept_mutex;
pthread_cond_t g_cond;
pthread_mutex_t g_mutex;

pthread_mutex_t client_mutex;

std::list<int> client_list;

// 程序退出
void prog_exit(int sig) {
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    std::cout << "program recv signal" << sig << "to exit" << std::endl;
    stop = 1;
    epoll_ctl(epfd, EPOLL_CTL_DEL, sfd, NULL);
    // 优雅断开
    shutdown(sfd, SHUT_RDWR);
    close(sfd);
    close(epfd);
    pthread_cond_destroy(&accept_cond);
    pthread_cond_destroy(&g_cond);

    pthread_mutex_destroy(&accept_mutex);
    pthread_mutex_destroy(&g_mutex);
    pthread_mutex_destroy(&client_mutex);
}
// 初始化
bool create_server_listener(const char *ip, int port) {
    sfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sfd == -1) return 0;
    int on = 1;
    // 设置端口复用
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
    setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, (char *)&on, sizeof(on));

    sockaddr_in addr;
    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if (bind(sfd, (sockaddr*)&addr, sizeof(addr)) == -1) return 0;
    if (listen(sfd, 64) == -1) return 0;
    // 创建epoll
    // 并且监听事件
    if ((epfd = epoll_create(EPOLL_SIZE)) == -1) return 0;
    epoll_event event;
    memset(&event, '\0', sizeof(event));
    event.data.fd = sfd;
    event.events = EPOLLIN | EPOLLRDHUP;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &event) == -1) return 0;
    return 1;
}

void release_client(int cfd) {
    if (epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, nullptr) == -1) {
        std::cout << "remove error" << std::endl;
    }
    close(cfd);
}
// 设置接收线程的函数
void *accept_thread_func(void *arg) {
    while (!stop) {
        // 给accpet 操作上锁
        pthread_mutex_lock(&accept_mutex);
        pthread_cond_wait(&accept_cond, &accept_mutex);
        sockaddr_in caddr;
        socklen_t size = sizeof(caddr);
        int cfd = accept(sfd, (sockaddr*)&caddr, &size);
        if (cfd == -1) continue;
        // accept 成功后, 解锁
        pthread_mutex_unlock(&accept_mutex);
        std::cout << "new client !" << inet_ntoa(caddr.sin_addr) << ":" << ntohs(caddr.sin_port) << std::endl;
        int option = fcntl(cfd, F_GETFL);
        int ret = fcntl(cfd, F_SETFL, option | O_NONBLOCK);
        if (ret == -1) continue;
        epoll_event event;
        memset(&event, '\0', sizeof(event));
        event.data.fd = cfd;
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &event) == -1) {
            std::cout << "epoll add error: fd = " << cfd << std::endl;
        }
    }
    return NULL;
}

void *work_thread_func(void *arg) {
    while (!stop) {
        int cfd;
        // 锁起来, 并且 pop
        pthread_mutex_lock(&client_mutex);
        while (client_list.empty()) pthread_cond_wait(&g_cond, &client_mutex);
        cfd = client_list.front();
        client_list.pop_front();
        pthread_mutex_unlock(&client_mutex);
        char buff[1024];
        std::string msg;
        while (1) {
            memset(buff, '\0', sizeof(buff));
            int len = recv(cfd, buff, 1023, 0);
            if (len == -1) {
                if (errno == EWOULDBLOCK) break;
            } else if (!len) {
                std::cout << "A client quit" << std::endl;
                break;
            }
            msg += buff;
        }
        for (auto &c : msg) {
            if (c >= 'a' && c <= 'z') c = 'A' + c - 'a';
        }
        while (1) {
            int len = send(cfd, msg.data(), msg.length(), 0);
            if (len == -1) {
                if (errno == EWOULDBLOCK) {
                    sleep(10);
                    continue;
                } else {
                    release_client(cfd);
                    break;
                }
            }
        }
    }
    return NULL;
}
// 以守护进程的方式运行
void daemen_run() {
    // 忽略子进程状态的改变
    pid_t pid;
    signal(SIGCHLD, SIG_IGN);
    pid = fork();
    if (pid < 0) {
        std::cout << "fork error" << std::endl;
        exit(-1);
    } else if (pid > 0) {
        // 父进程直接退出,
        exit(0);
    }
    setsid();
    // 把 输入输出重定向到子进程
    int fd;
    fd = open("/dev/null", O_RDWR, 0);
    if (fd != -1)
    {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
    }
    if (fd > 2) close(fd);
}

int main(int ac, const char **av) {
    if (ac == 2 && !strcmp(av[1], "-d")) daemen_run();
    if (!create_server_listener(IP, PORT)) {
        std::cout << "create listen error" << std::endl;
        return -1;
    }
    signal(SIGCHLD, SIG_DFL);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, prog_exit);
    signal(SIGTERM, prog_exit);
    // 线程相关的初始化
//    pthread_mutex_t accept_mutex;
//    pthread_cond_t g_cond;
//    pthread_mutex_t g_mutex;
//
//    pthread_mutex_t client_mutex;
    pthread_mutex_init(&accept_mutex, nullptr);
    pthread_mutex_init(&g_mutex, nullptr);
    pthread_mutex_init(&client_mutex, nullptr);
    pthread_cond_init(&g_cond, nullptr);
    pthread_cond_init(&accept_cond, nullptr);
    pthread_create(&accept_thread, nullptr, accept_thread_func, nullptr);
    for (int i = 0; i < WORKER_THREAD_NUM; ++i) pthread_create(&threads[i], nullptr, work_thread_func, nullptr);
    while (!stop) {
        epoll_event events[EPOLL_SIZE];
        int num = epoll_wait(epfd, events, EPOLL_SIZE, -1);
        if (num < 0) {
            std::cout << "epoll_wait error" << std::endl;
            continue;
        }
        if (!num) continue;
        for (int i = 0; i < num; ++i) {
            if (events[i].data.fd == sfd) pthread_cond_signal(&accept_cond);
            else {
                pthread_mutex_lock(&client_mutex);
                client_list.push_back(events[i].data.fd);
                pthread_mutex_unlock(&client_mutex);
                pthread_cond_signal(&g_cond);
            }
        }
    }
    return 0;
}