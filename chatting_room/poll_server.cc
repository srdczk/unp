#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <string>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#define IP "127.0.0.1"
#define PORT 9999
#define MAX_CLIENT 5
#define BUFSIZE 1024
#define FD_MAX 65535

int set_nonblocking(int fd) {
    int option = fcntl(fd, F_GETFL);
    int new_option = option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return option;
}
void error_handling(const char *msg) {
    perror(msg);
    exit(-1);
}
// 客户端的数据
struct client_data {
    // 地址
    sockaddr_in addr;
    // 读写缓存区
    char *writebuf;
    char readbuf[BUFSIZE];
};
// 客户端程序, poll数组两个, 分别监听 标准输入和 socketfd
// 直接用splice 0 拷贝
int main() {
    int sfd, user_count = 0;
    sockaddr_in addr;
    // 将读写设置为 nonblocking 的
    // 存放数据
    pollfd fds[MAX_CLIENT + 1];
    // 每个客户端 sock 的值可以直接作为索引, 牺牲空间换取时间
    client_data *users = new client_data[FD_MAX];
    if ((sfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) error_handling("socket error");
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, IP, &addr.sin_addr);
    if (bind(sfd, (sockaddr*)&addr, sizeof(addr)) == -1) error_handling("bind error");
    if (listen(sfd, MAX_CLIENT) == -1) error_handling("listen error");
    fds[0].fd = sfd;
    fds[0].events = POLLIN | POLLERR;
    fds[0].revents = 0;
    for (int i = 1; i < MAX_CLIENT + 1; ++i) {
        fds[i].fd = -1;
        fds[i].events = 0;
    }
    while (1) {
        int ret = poll(fds, MAX_CLIENT + 1, -1);
        if (ret < 0) {
            printf("poll failure\n");
            break;
        }
        for (int i = 0; i < user_count + 1; ++i) {
            if (fds[i].fd == sfd && fds[i].revents & POLLIN) {
                // 有新连接来了
                sockaddr_in clnt_addr;
                socklen_t size = sizeof(clnt_addr);
                int clnt_fd = accept(sfd, (sockaddr*)&clnt_addr, &size);
                // 寻找新的地方插入
                if (clnt_fd < 0) continue;
                if (user_count >= MAX_CLIENT) {
                    // 如果连接数已经达到了最满
                    std::string s = "max line, sorry";
                    write(clnt_fd, s.data(), s.length());
                    close(clnt_fd);
                    continue;
                }
                // 在user_count地方插入
                users[clnt_fd].addr = clnt_addr;
                user_count++;
                set_nonblocking(clnt_fd);
                fds[user_count].fd = clnt_fd;
                fds[user_count].events = POLLIN | POLLRDHUP;
                fds[user_count].revents = 0;
                // 建立了新的连接
                printf("a new Client\n");
            } else if (fds[i].revents & POLLERR) {
                //
                printf("get an error\n");
                char error[100];
                memset(error, '\0', 100);
                socklen_t len = sizeof(error);
                if (getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
                    printf("opt fail\n");
                }
                continue;
            } else if (fds[i].revents & POLLRDHUP) {
                printf("a client quit!\n");
                users[fds[i].fd] = users[fds[user_count].fd];
                close(fds[i].fd);
                fds[i] = fds[user_count];
                i--;
                user_count--;
            } else if (fds[i].revents & POLLIN) {
                // 若果有客户端 发送了消息, 则要传送给所有客户端
                client_data data = users[fds[i].fd];
                memset(data.readbuf, '\0', BUFSIZE);
                int len = read(fds[i].fd, data.readbuf, BUFSIZE);
                if (len <= 0) {
                    printf("read error\n");
                    continue;
                }
                // 通知其他的socket准备写数据
                for (int j = 1; j < user_count + 1; ++j) {
                    if (j == i) continue;
                    fds[j].events |= ~POLLIN;
                    fds[j].events |= POLLOUT;
                    // 需要输出
                    users[fds[j].fd].writebuf = data.readbuf;
                }
            } else if (fds[i].revents & POLLOUT) {
                // 输出, 将out重置为监听 in 事件
                char *buf = users[fds[i].fd].writebuf;
                if (!buf) continue;
                // 向其写数据
                write(fds[i].fd, buf, strlen(buf));
                users[fds[i].fd].writebuf = NULL;
                fds[i].events |= ~POLLOUT;
                fds[i].events |= POLLIN;
            }
        }
    }
    delete []users;
    close(sfd);
    return 0;
}