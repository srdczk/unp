#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/stat.h>
using namespace std;

#define CRLF "\r\n"
#define BUFSIZE 4096
#define PORT 9999
vector<string> split(string &s, const string &p) {
    vector<string> res;
    char *ss = const_cast<char *>(s.data()), *t;
    const char *pp = p.data();
    if (t = strtok(ss, pp)) res.push_back(t);
    while (t = strtok(NULL, pp)) res.push_back(t);
    return res;
}
void error_handling(const string &msg) {
    perror(msg.data());
    exit(-1);
}

// 目前只支持 ipv4
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}
int main() {
    int sfd, cfd, str_len = 0, len;
    sockaddr_in saddr, caddr;
    socklen_t size = sizeof(caddr);
    bool x = 0;
    char buff[BUFSIZE];
    if ((sfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) error_handling("socket error");
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_port = htons(PORT);
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sfd, (sockaddr*)&saddr, sizeof(saddr)) == -1) error_handling("bind error");
    if (listen(sfd, 5) == -1) error_handling("listen error");
    if ((cfd = accept(sfd, (sockaddr*)&caddr, &size)) == -1) error_handling("accept error");
    // 将 cfd 设置为非阻塞的
    setnonblocking(cfd);
    // 当第一次开始接收到有内容的东西开始
    while (1) {
        len = read(cfd, buff, BUFSIZE);
        // 已经开始接受到, 有意义的东西了
        if (x) {
            if (len < 0) {
                if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                    break;
                }
            } else if (!len) break;
            else {
                str_len += len;
            }
        } else {
            if (len > 0) {
                x = 1;
                str_len += len;
            } else if (len < 0) continue;
            else {
                break;
            }
        }
    }
    printf("%d\n", str_len);
    buff[str_len] = '\0';
    printf("%s", buff);
    close(cfd);
    close(sfd);
    return 0;
}