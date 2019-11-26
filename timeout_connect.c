#include <sys/socket.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>

#define PORT 9999
#define IP "127.0.0.1"
// 通过设置socket 的SO_SNDTIMEEO来实现connect的超时时间
int timeout_connect(const char *ip, int port, int time) {
    int cfd, ret = 0;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    // 转换ip地址
    inet_pton(AF_INET, ip, &addr.sin_addr);
    cfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(cfd != -1);
    struct timeval timeout;
    timeout.tv_sec = time;
    timeout.tv_usec = 0;
    socklen_t len = sizeof(timeout);
    ret = setsockopt(cfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);
    assert(ret != -1);
    ret = connect(cfd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret == -1) {
        // 超时信号, 可以开始处理定时任务
        if (errno == EINPROGRESS) {
            printf("connect timeout!\n");
        }
        return -1;
    }
    return cfd;
}

int main() {
    if (timeout_connect(IP, PORT, 10) == -1) return 1;
    return 0;
}