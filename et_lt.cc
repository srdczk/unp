#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <string>
#define MAX_EVENTS 1024
#define PORT 9999
#define EPOLL_SIZE 5
#define BUFSIZE 10
// epoll 的最大监听事件
// 设置文件为非阻塞的
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void error_handling(const std::string &s) {
    perror(s.data());
    exit(-1);
}
// 添加文件描述符到epoll监听上, 选择是否用边缘触发模式
void addfd(int epfd, int fd, bool et) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if (et) event.events |= EPOLLET;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
    // 将其设为非阻塞的
    setnonblocking(fd);
}
// lt 水平触发
void lt(epoll_event *events, int size, int epfd, int sfd) {
    char buff[BUFSIZE];
    int len;
    // 返回文件描述符
    for (int i = 0; i < size; ++i) {
        int cfd = events[i].data.fd;
        // 如果等于sockfd, 则说明有新的连接过来, 开始处理 :
        if (cfd == sfd) {
            sockaddr_in newAddr;
            socklen_t size = sizeof(newAddr);
            int newFd = accept(sfd, (sockaddr *)&newAddr, &size);
            addfd(epfd, newFd, 0);
        } else if (events[i].events & EPOLLIN) {
            // socket 读缓存中还有未读的数据, 就会被触发
            // 将缓冲区设置的小一点
            printf("event lt p!\n");
            memset(buff, '\0', BUFSIZE);
            len = read(cfd, buff, BUFSIZE - 1);
            if (len <= 0) {
                close(cfd);
                continue;
            }
            printf("receive: %s\n", buff);
        } else printf("Something else\n");
    }
}
// 边缘触发
void et(epoll_event *events, int size, int epfd, int sfd) {
    char buff[BUFSIZE];
    for (int i = 0; i < size; ++i) {
        int cfd = events[i].data.fd;
        if (cfd == sfd) {
            sockaddr_in newAddr;
            socklen_t size = sizeof(newAddr);
            int newFd = accept(sfd, (sockaddr *)&newAddr, &size);
            addfd(epfd, newFd, 1);
        } else if (events[i].events & EPOLLIN) {
            // 有东西需要读入, 而不会被调用多次
            while (1) {
                memset(buff, '\0', BUFSIZE);
                int len = read(cfd, buff, BUFSIZE);
                if (len < 0) {
                    if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                        break;
                    }
                    close(cfd);
                    break;
                } else if (!len) close(cfd);
                else printf("receive:%s\n", buff);
            }
        } else {
            printf("Something else\n");
        }
    }
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
    addfd(epfd, sfd, 1);
//    addfd(epfd, sfd, 0);
    while (1) {
        int size = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (!size) continue;
        if (size < 0) break;
        lt(events, size, epfd, sfd);
//        et(events, size, epfd, sfd);

    }
    close(sfd);
    return 0;
}