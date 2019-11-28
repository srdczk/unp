#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>
#include <time.h>

#define BUFSIZE 1024
#define EPOLL_SIZE 1024
#define PORT 9999
struct my_event {
    int fd;
    int events;
    void *arg;
    void (*callback)(int fd, int events, void *arg);
    int index;
    // 记录是否在监听
    char buff[BUFSIZE];
    int len;
    // 记录每次加入红黑树
    long last_active;
};
int g_epfd, cnt = 0;
// 事件集合
struct my_event g_events[EPOLL_SIZE + 1];
int set_nonblocking(int fd);
void error_handler(const char *msg);
void init_sfd();
void accept_connect(int sfd, int events, struct my_event *ev);
// 设置事件, 回调函数, 监听 fd
void event_set(struct my_event *ev, int events, void (*callback)(int , int , void *), int fd);
// 红黑树添加事件
void event_add(int events, struct my_event *ev);
// 红黑树删除事件
void event_del(struct my_event *ev);
void recv_data(int cfd, int events, struct my_event *ev);
void send_data(int cfd, int events, struct my_event *ev);

int set_nonblocking(int fd) {
    int option = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, option | O_NONBLOCK);
    return option;
}

void error_handler(const char *msg) {
    perror(msg);
    exit(-1);
}

void init_sfd() {
    int sfd;
    struct sockaddr_in addr;
    // 建立监听, 并且端口复用
    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if ((sfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) error_handler("socket error");
    // 设置非阻塞
    set_nonblocking(sfd);
    // 设置端口复用
    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
    // 绑定, 红黑树 添加
    if (bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) error_handler("bind error");
    if (listen(sfd, 128) == -1) error_handler("listen error");
    if ((g_epfd = epoll_create(EPOLL_SIZE)) == -1) error_handler("epoll create error");
    event_set(&g_events[EPOLL_SIZE], EPOLLIN, accept_connect, sfd);
    // 红黑树上添加节点
    event_add(EPOLLIN, &g_events[EPOLL_SIZE]);
}
void accept_connect(int sfd, int events, struct my_event *ev) {
    int cfd;
    struct sockaddr_in caddr;
    socklen_t size = sizeof(caddr);
    if ((cfd = accept(sfd, (struct sockaddr*)&caddr, &size)) == -1) error_handler("accept error");
    printf("A new client\n");
    event_set(&g_events[cnt], EPOLLIN, recv_data, cfd);
    g_events[cnt].index = cnt;
    event_add(events, &g_events[cnt]);
    cnt++;
}
void event_set(struct my_event *ev, int events, void (*callback)(int , int , void *), int fd) {
    ev->callback = callback;
    ev->fd = fd;
    ev->events = events;
    ev->arg = ev;
    ev->last_active = time(NULL);
}

void event_add(int events, struct my_event *ev) {
    struct epoll_event event;
    event.events = events;
    event.data.ptr = ev;
    if (epoll_ctl(g_epfd, EPOLL_CTL_ADD, ev->fd, &event) == -1) error_handler("epoll add error");
}
void event_del(struct my_event *ev) {
    if (epoll_ctl(g_epfd, EPOLL_CTL_DEL, ev->fd, NULL) == -1) error_handler("epoll del error");
}

void recv_data(int cfd, int events, struct my_event *ev) {
    memset(ev->buff, '\0', BUFSIZE);
    ev->len = recv(cfd, ev->buff, BUFSIZE, 0);
    printf("Receive :%s\n", ev->buff);
    if (ev->len < 0) {
        if (errno != EINTR && errno != EAGAIN) {
            event_del(ev);
            close(cfd);
            g_events[ev->index] = g_events[--cnt];
            g_events[ev->index].index = ev->index;
        }
    } else if (!ev->len) {
        event_del(ev);
        close(cfd);
        g_events[ev->index] = g_events[--cnt];
        g_events[ev->index].index = ev->index;
    } else {
        // 修改
        event_del(ev);
        event_set(ev, EPOLLOUT, send_data, cfd);
        event_add(EPOLLOUT, ev);
    }
}

void send_data(int cfd, int events, struct my_event *ev) {
    for (int i = 0; i < ev->len; ++i) {
        if (ev->buff[i] >= 'a' && ev->buff[i] <= 'z') ev->buff[i] = 'A' + ev->buff[i] - 'a';
    }
    printf("Send :%s\n", ev->buff);
    ev->len = send(cfd, ev->buff, ev->len, 0);
    if (ev->len < 0) {
        if (errno != EINTR && errno != EAGAIN) {
            event_del(ev);
            close(cfd);
            g_events[ev->index] = g_events[--cnt];
            g_events[ev->index].index = ev->index;
        }
    } else {
        // 重新设置为 read
        event_del(ev);
        event_set(ev, EPOLLIN, recv_data, cfd);
        event_add(EPOLLIN, ev);
    }
}

int main() {
    struct epoll_event events[EPOLL_SIZE];
    init_sfd();
    int check = 0;
    while (1) {
        int num = epoll_wait(g_epfd, events, EPOLLIN | EPOLLOUT, -1);
        long cur = time(NULL);
        for (int i = 0; i < 100; ++i, ++check) {
            if (check > cnt - 1) check = 0;
            if (check > cnt - 1) continue;
            // 断开超过一分钟的连接
            if (cur - g_events[check].last_active > 60) {
                event_del(&g_events[check]);
                close(g_events[check].fd);
                g_events[check] = g_events[--cnt];
                g_events[check].index = check;
            }
        }
        if (num < 0 && errno != EAGAIN) {
            printf("epoll wait fail\n");
            break;
        }
        for (int i = 0; i < num; ++i) {
            struct epoll_event *p = &events[i];
            struct my_event *ev = (struct my_event*)p->data.ptr;
            if ((p->events & EPOLLIN) && (ev->events & EPOLLIN)) {
                ev->callback(ev->fd, ev->events, ev->arg);
            } else if ((p->events & EPOLLOUT) && (ev->events & EPOLLOUT)) {
                ev->callback(ev->fd, ev->events, ev->arg);
            }
        }
    }
    return 0;
}


