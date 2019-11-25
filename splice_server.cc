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
#define BUFSIZE 1024
void error_handling(const std::string &s) {
    perror(s.data());
    exit(-1);
}
int main() {
    int sfd, cfd, pipefd[2], ret;
    sockaddr_in addr, caddr;
    socklen_t size = sizeof(caddr);
    if ((sfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) error_handling("socket error");
    memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (bind(sfd, (sockaddr*)&addr, sizeof(addr)) == -1) error_handling("bind error");
    if (listen(sfd, 5) == -1) error_handling("listen error");
    if ((cfd = accept(sfd, (sockaddr*)&caddr, &size)) == -1) error_handling("accept error");
    // 管道
    ret = pipe(pipefd);
    assert(ret != -1);
    // 管道输入
    // 回声服务器
    ret = splice(cfd, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
    assert(ret != -1);
    ret = splice(pipefd[0], NULL, cfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
    assert(ret != -1);
    // 把标准输出重定向到sfd上
    close(cfd);
    close(sfd);
    return 0;
}