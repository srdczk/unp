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
#define ip "127.0.0.1"
#define BUFSIZE 1024
void error_handling(const std::string &s) {
    perror(s.data());
    exit(-1);
}
int main() {
    int cfd, len;
    char buff[BUFSIZE];
    sockaddr_in addr;
    if ((cfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) error_handling("socket error");
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    if (connect(cfd, (sockaddr*)&addr, sizeof(addr)) == -1) error_handling("");
    while (1) {
        printf("Please input your chose(q or Q quit):\n");
        memset(buff, '\0', BUFSIZE);
        len = read(STDIN_FILENO, buff, BUFSIZE);
        if (!strcmp(buff, "Q\n") || !strcmp(buff, "q\n") || len == -1) break;
        write(cfd, buff, len);
        memset(buff, '\0', BUFSIZE);
        while ((len = read(cfd, buff, BUFSIZE)) != 0 && len != -1) {
            printf("%s\n", buff);
        }
    }
    close(cfd);
    return 0;
}