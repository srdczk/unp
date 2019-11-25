#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
// splice 在两个文件描述符中移动数据, 0 拷贝操作
#define PORT 9999
#define file_name "test.txt"
#define BUFSIZE 1024

int setnonblocking(int fd) {
    int option = fcntl(fd, F_GETFL);
    int new_option = option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return option;
}
// 非阻塞的 connect 实现, 设置超时时间 time (毫秒)
int unblock_connect(const char *ip, int port, int time) {
    int cfd;
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &addr.sin_addr);
    cfd = socket(PF_INET, SOCK_STREAM, 0);
    // 设置非阻塞
    int option = setnonblocking(cfd);
    int ret = connect(cfd, (sockaddr*)&addr, sizeof(addr));
    if (!ret) {
        // 如果连接成功
        printf("connect with server immediately\n");
        //恢复属性
        fcntl(cfd, F_SETFL, option);
        return cfd;
    } else if (errno != EINPROGRESS) {
        // 如果没有连接, 且当errno
        printf("unblock connect not support\n");
        return -1;
    }
    fd_set readfds;
    fd_set writefds;
    timeval timeout;
    FD_ZERO(&readfds);
    FD_SET(cfd, &writefds);
    timeout.tv_sec = time;
    timeout.tv_usec = 0;
    ret = select(cfd + 1, NULL, &writefds, NULL, &timeout);
    if (ret <= 0) {
        printf("timeout!\n");
        close(cfd);
        return -1;
    }
    if (!FD_ISSET(cfd, &writefds)) {
        printf("no events\n");
        close(cfd);
        return -1;
    }
    int error = 0;
    socklen_t len = sizeof(error);
    // 清楚 cfd 的错误
    if (getsockopt(cfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        printf("getSocket error");
        close(cfd);
        return -1;
    }
    if (error) {
        printf("select error\n");
        close(cfd);
        return -1;
    }
    printf("connection ready after with socket\n");
    fcntl(cfd, F_SETFL, option);
    return cfd;
}
void error_handling(const std::string &s) {
    perror(s.data());
    exit(-1);
}

int main() {
    const char *ip = "127.0.0.1";
    int sfd = unblock_connect(ip, PORT, 10);
    if (sfd < 0) {
        return 1;
    }
    close(sfd);
    return 0;
}