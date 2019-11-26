#include "util.h"
#include <sys/signal.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#define EPOLL_SIZE 2000
#define FD_LIMIT 65536
#define TIMESLOT 5
#define PORT 9999
// 每 5 秒发一次信号
// 每一个定时事件的产生都会触发 timer_handler

// 时间升序链表
static time_heap heap;
// 下次实现时间堆
static int epfd;
static int pipefd[2];

void error_handling(const char *msg) {
    perror(msg);
    exit(-1);
}
// 把 时间堆 集成到 web_server 里边
// 信号处理函数, 把信号输出给管道
void sig_handler(int sig) {
    int old_errno = errno;
    // 输入管道入口, (sig 是另外一个进程)
    int msg = sig;
    // 只传送第一个值, 只在小端机器能这样
    char *t = (char *)&msg;
    int ret = send(pipefd[1], t, 1, 0);
    errno = old_errno;
}
// web server + 时间堆处理
// 信号处理函数 --> 把信号传给管道, 管道出口处由 epoll 进行统一处理
void addsig(int sig) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    // 全部置为0
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    if (sigaction(sig, &sa, NULL) == -1) error_handling("sigaction error");
}

void callback(client_data *data) {
    // 关闭这个client
    if (!data) return;
    if (epoll_ctl(epfd, EPOLL_CTL_DEL, data->cfd, nullptr) == -1) error_handling("epoll del error");
    // 关闭这个连接
    // 由于 tick 中已经去除了这个值了
    close(data->cfd);
}

// 每次处理超时的任务 --> 关闭连接
void timer_handler() {
    heap.tick();
    alarm(TIMESLOT);
}

int set_nonblocking(int fd) {
    int option = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, option | O_NONBLOCK);
    return option;
}

void addfd(int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    // 边缘触发模式
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
    set_nonblocking(fd);
}

int main() {
    int sfd;
    // epoll 事件集合
    epoll_event events[EPOLL_SIZE];
    sockaddr_in addr;
    // client 直接用其 cfd 表示
    client_data *clients = new client_data[FD_LIMIT];
    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if ((sfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) error_handling("socket error");
    if (bind(sfd, (sockaddr*)&addr, sizeof(addr)) == -1) error_handling("bind error");
    if (listen(sfd, 64) == -1) error_handling("listen error");
    if ((epfd = epoll_create(EPOLL_SIZE)) == -1) error_handling("epoll create error");
    // 在 epoll 上加上监听
    // socket 中建立双向管道
    if (socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd));
    set_nonblocking(pipefd[1]);
    addfd(pipefd[0]);
    addfd(sfd);
    addsig(SIGALRM);
    bool timeout = false;
    // 开始定时
    alarm(TIMESLOT);
    while (1) {
        int num = epoll_wait(epfd, events, EPOLL_SIZE, -1);
        if (num < 0 && errno != EINTR) {
            printf("epoll wait failure\n");
            break;
        }
        // 可以直接研究负载均衡了
        for (int i = 0; i < num; ++i) {
            epoll_event *ptr = &events[i];
            // 只处理EPOLLIN事件
            if (!(ptr->events & EPOLLIN)) continue;
            if (ptr->data.fd == sfd) {
                // 则要处理新的请求了
                int cfd;
                sockaddr_in caddr;
                socklen_t size = sizeof(caddr);
                if ((cfd = accept(sfd, (sockaddr*)&caddr, &size)) == -1) continue;
                // 建立新的client_data, 在升序链表中
                time_t cur = time(nullptr);
                time_node node;
                node.data = &clients[cfd];
                node.expire = cur + 3 * TIMESLOT;
                node.callback = callback;
                // 如果建立连接之后 15 s 还未有 进行通信 , 则断开连接
                clients[cfd].cfd = cfd;
                clients[cfd].addr = caddr;

                // 输出 有一个新的连接生成了
                char ip[64];
                memset(ip, '\0', sizeof(ip));
                printf("New Client IP: %s, Port: %d, cfd = %d\n",
                       inet_ntop(AF_INET, &caddr.sin_addr.s_addr, ip, sizeof(ip)),
                       ntohs(caddr.sin_port), cfd);
                // 时间链表上, 插入此节点
                // 加入epoll事件监听
                clients[cfd].index = heap.insert(node);
                addfd(cfd);
            } else if (pipefd[0] == ptr->data.fd) {
                // 如果是管道的值
                char buff[BUFSIZE];
                int len = recv(pipefd[0], buff, BUFSIZE, 0);
                printf("len: %d\n", len);
                if (len <= 0) continue;
                for (int j = 0; j < len; ++j) {
                    // 定时轮询时间 需要开启
                    printf("sig: %x ", buff[j]);
                    if (buff[j] == SIGALRM) timeout = true;
                }
            } else {
                // 如果是其他事件, 则 返回她们所输入的东西的大写版本
                int cfd = ptr->data.fd;
                // 直接读取到client的buff里
                // 输入, 输出, 还需要调整
                int index = clients[cfd].index;
                memset(clients[cfd].buff, '\0', BUFSIZE);
                int len = recv(cfd, clients[cfd].buff, BUFSIZE - 1, 0);
                if (len < 0) {
                    if (errno == EAGAIN) {
                        // 删除这个点
                        heap.del(index);
                        callback(&clients[cfd]);
                    }
                } else if (!len) {
                    heap.del(index);
                    callback(&clients[cfd]);
                } else {
                    // timer -> adjust
                    // timer 进行 adjust
                    // 向原来的写入
                    char *buff = clients[cfd].buff;
                    printf("receive: %s\n", buff);
                    for (int j = 0; j < strlen(buff); ++j) {
                        if (buff[j] >= 'a' && buff[j] <= 'z') buff[j] = 'A' + buff[j] - 'a';
                    }
                    send(cfd, buff, strlen(buff), 0);
                    time_t cur = time(nullptr);
                    heap.adjust(index, cur + 3 * TIMESLOT);
                }
                if (timeout) {
                    // 进行处理
                    timer_handler();
                    timeout = false;
                }
            }
        }
    }
    delete [] clients;
    close(sfd);
    close(pipefd[0]);
    close(pipefd[1]);
    return 0;
}