#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <string>
#define MAX_EVENTS 1024
#define PORT 9999
#define EPOLL_SIZE 5
#define BUFSIZE 1024

struct fds {
    int sockfd;
    int epfd;
};

// 设置非阻塞
int setnonblocking(int fd) {
    int option = fcntl(fd, F_GETFL);
    int new_option = option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return option;
}

// 设置oneshot , 一个描述符只能被一个线程使用
void addfd(int epfd, int fd, bool oneshot) {
    // 默认et模式, 非阻塞
    epoll_event event;
    event.data.fd = fd;
    // setContentLength -> 加上one-shot
    event.events = EPOLLIN | EPOLLET;
    if (oneshot) event.events |= EPOLLONESHOT;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
    // 设置成非阻塞模式
    setnonblocking(fd);
}
// 当一个线程要结束的时候, 重新注册oneshot
void reset_oneshot(int epfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event);
}
// 工作线程
void *work(void *arg) {
    char buff[BUFSIZE];
    int sockfd = ((fds*)arg)->sockfd;
    int epfd = ((fds*)arg)->epfd;
    // 读取数据
    while (1) {
        memset(buff, '\0', BUFSIZE);
        int len = read(sockfd, buff, BUFSIZE - 1);
        if (!len) {
            close(sockfd);
            break;
        } else if (len < 0) {
            // 重置oneshot, 让其他线程能够给占有
            if (errno = EAGAIN) {
                reset_oneshot(epfd, sockfd);
                printf("Good bye\n");
                break;
            }
        } else {
            printf("Receive: %s\n", buff);
            sleep(2);
            // 模拟处理过程(监听socket不能置为oneshot)
            printf("I sleep 2 seconds\n");
        }
    }
    printf("end receive\n");
}
// 错误处理
void error_handling(const std::string &s) {
    perror(s.data());
    exit(-1);
}
int main() {
    int sfd, epfd;
    sockaddr_in addr;
    epoll_event events[MAX_EVENTS];
    if ((sfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) error_handling("socket error");
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sfd, (sockaddr*)&addr, sizeof(addr)) == -1) error_handling("bind error");
    if (listen(sfd, 5) == -1) error_handling("listen error");
    if ((epfd = epoll_create(EPOLL_SIZE)) == -1) error_handling("epoll error");
    // 主监听的 sfd不能够置为 sfd
    addfd(epfd, sfd, 0);
    while (1) {
        int size = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (size < 0) break;
        for (int i = 0; i < size; ++i) {
            int cfd = events[i].data.fd;
            if (cfd == sfd) {
                // 接入监听事件
                sockaddr_in caddr;
                socklen_t socklen = sizeof(caddr);
                int new_fd = accept(sfd, (sockaddr*)&caddr, &socklen);
                addfd(epfd, new_fd, 1);
            } else if (events[i].events & EPOLLIN) {
                // 建立处理线程
                pthread_t pthread;
                fds thread_fds{events[i].data.fd, epfd};
                pthread_create(&pthread, NULL, work, (void *)&thread_fds);
            } else printf("Something else\n");
        }
    }
    close(sfd);
    return 0;
}