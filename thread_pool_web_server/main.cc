#include <cstdlib>
#include <cerrno>
#include "http.h"
#include "threadpool.h"
#define EPOLL_SIZE 1024
#define FD_LIMIT 65536
void error_handling(const char *msg) {
    perror(msg);
    exit(-1);
}
int main() {

    int sfd;
    sockaddr_in addr;
    epoll_event events[EPOLL_SIZE];
    thread_pool<http_connect> *pool = new thread_pool<http_connect>();
    http_connect *connects = new http_connect[FD_LIMIT];
    if ((sfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) error_handling("socket error");
    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sfd, (sockaddr*)&addr, sizeof(addr)) == -1) error_handling("bind error");
    if (listen(sfd, 128) == -1) error_handling("listen error");
    if ((http_connect::m_epfd = epoll_create(EPOLL_SIZE)) == -1) error_handling("epoll create error");
    add_fd(http_connect::m_epfd, sfd);
    while (true) {
        int num = epoll_wait(http_connect::m_epfd, events, EPOLL_SIZE, -1);
        if (num < 0 && errno != EAGAIN) {
            printf("epoll wait fail\n");
            break;
        }
        for (int i = 0; i < num; ++i) {
            epoll_event *ptr = &events[i];
            if (!(ptr->events & EPOLLIN)) continue;
            if (ptr->data.fd == sfd) {
                sockaddr_in caddr;
                int cfd;
                socklen_t size = sizeof(caddr);
                if ((cfd = accept(sfd, (sockaddr*)&caddr, &size)) == -1) continue;
                printf("A new client !\n");
                add_fd(http_connect::m_epfd, cfd);
                connects[cfd].init(cfd, &caddr);
                printf("cfd: %d\n", connects[cfd].cfd);
            } else {
                if (connects[ptr->data.fd].read()) {
                    pool->append(&connects[ptr->data.fd]);
                } else {
                    connects[ptr->data.fd].disconnect();
                }
            }
        }
    }
    close(http_connect::m_epfd);
    close(sfd);
    delete [] connects;
    delete pool;
    return 0;
}