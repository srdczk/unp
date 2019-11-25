#include <sys/socket.h>
#include <signal.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define PORT 9999
#define BUFSIZE 1024
#define EPOLL_SIZE 2000
// 统一事件源 --> 将信号通过管道统一管理进行监听
static int pipefd[2];
// 信号handler -> 把信号输入管道
void sig_handler(int sig) {
    // 设置之前的errno
    int old_errno = errno;
    // 信号只用一个 char 类型的代表
    int msg = sig;
    // 把信号输入到管道进行处理
    send(pipefd[1], (char *) &msg, 1, 0);
    // 重置原来的errno
    errno = old_errno;
}
int addsig(int sig) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

int addfd(int fd, int epfd) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
}

int set_nonblocking(int fd) {
    int option = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, option | O_NONBLOCK);
    return option;
}

void error_handling(const char *msg) {
    perror(msg);
    exit(-1);
}

int main() {
    int sfd, epfd;
    struct sockaddr_in addr;
    struct epoll_event events[EPOLL_SIZE + 2];
    if ((sfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) error_handling("socket error");
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) error_handling("bind error");
    if (listen(sfd, 64) == -1) error_handling("listen error");
    if ((epfd = epoll_create(EPOLL_SIZE)) == -1) error_handling("epoll create error");
    addfd(sfd, epfd);
    if (pipe(pipefd) == -1) error_handling("pipe error");
    int stop = 0;
    while (!stop) {
        int num = epoll_wait(epfd, events, EPOLL_SIZE, -1);
        if (num < 0) break;
        for (int i = 0; i < num; ++i) {
            struct epoll_event *ptr = &events[i];
            // 只处理EPOLLIN事件
            if (!(ptr->data.fd & EPOLLIN)) continue;
            else if (ptr->data.fd == sfd) {
                struct sockaddr_in caddr;
                socklen_t size = sizeof(caddr);
                int cfd = accept(sfd, (struct sockaddr*)&caddr, &size);
                if (cfd == -1) continue;
                addfd(cfd, epfd);
                // 等于管道输出
            } else if (ptr->data.fd == pipefd[0]) {
                char sig[BUFSIZE];
                memset(sig, '\0', sizeof(sig));
                int len = recv(pipefd[0], sig, sizeof(sig), 0);
                if (len <= 0) continue;
                else {
                    for (int i = 0; i < strlen(sig); ++i) {
                        switch (sig[i]) {
                            case SIGCHLD:
                            case SIGURG:
                                continue;
                            case SIGTERM:
                            case SIGINT:
                                stop = 1;
                        }
                    }
                }
            }
        }
    }
    close(sfd);
    close(pipefd[0]);
    close(pipefd[1]);
    return 0;
}