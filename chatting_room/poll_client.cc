#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#define IP "127.0.0.1"
#define PORT 9999
#define BUFSIZE 1024

void error_handling(const char *msg) {
    perror(msg);
    exit(-1);
}
// 客户端程序, poll数组两个, 分别监听 标准输入和 socketfd
// 直接用splice 0 拷贝

int main() {
    int cfd, pipefd[2];
    sockaddr_in addr;
    pollfd fds[2];
    char buff[BUFSIZE];
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, IP, &addr.sin_addr);
    if ((cfd = socket(PF_INET, SOL_SOCKET, 0)) == -1) error_handling("socket error");
    if (connect(cfd, (sockaddr*)&addr, sizeof(addr)) == -1) error_handling("connect error");
    // 设置poll 的数组
    // 第一个监听标准输入, 第二个监听fd,events 监听的events, revens 返回的events
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    // 监听是否断开了连接
    fds[1].fd = cfd;
    fds[1].events = POLLIN | POLLRDHUP;
    fds[1].revents = 0;
    if (pipe(pipefd) == -1) error_handling("pipe error");
    while (1) {
        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            printf("poll failue\n");
            break;
        }
        if (fds[1].revents & POLLIN) {
            // 若果有输入
            memset(buff, '\0', BUFSIZE);
            read(cfd, buff, BUFSIZE);
            printf("receive: %s\n", buff);
        }
        // 断开连接
        if (fds[1].revents & POLLRDHUP) {
            printf("Server close the connection\n");
            break;
        }
        if (fds[0].revents & POLLIN) {
            // 如果有标准输入
            //输出
            // 定向到管道输入
            // 系统暂时 operation not permitted, 没查到原因, 先不用splice
//            if (splice(0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE | SPLICE_F_NONBLOCK) == -1) error_handling("splice error");
//            // 定向到fd输出
//            if (splice(pipefd[0], NULL, cfd, NULL, 32768, SPLICE_F_MOVE | SPLICE_F_MORE) == -1) error_handling("splice error");
            memset(buff, '\0', BUFSIZE);
            read(STDIN_FILENO, buff, BUFSIZE);
            // 写出去
            write(cfd, buff, strlen(buff));
        }
    }
    close(cfd);
    return 0;
}