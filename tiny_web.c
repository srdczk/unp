#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/stat.h>

#define BUFSIZE 1024

void error_handling(const char *);

int main(int ac, char **av) {
    // 服务端的socket 句柄和客户端的socket句柄
    int sfd, cfd;
    // 服务端的地址 和客户端的地址
    struct sockaddr_in saddr, caddr;
    socklen_t size = sizeof(caddr);
    // 输入格式 ./xxx 127.0.0.1 8189
    if (ac < 3) error_handling("input error");
    if ((sfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) error_handling("socket error");
    memset(&saddr, 0, sizeof(saddr));
    //ipv4
    saddr.sin_family = AF_INET;
    // 转为网路传输的格式
    saddr.sin_port = htons(atoi(av[2]));
    inet_pton(AF_INET, av[1], &saddr.sin_addr);
    // 绑定
    if (bind(sfd, (struct sockaddr*)&saddr, sizeof(saddr)) == -1) error_handling("bind error");
    // 监听
    if (listen(sfd, 5) == -1) error_handling("listen error");
    // accept 阻塞
    if ((cfd = accept(sfd, (struct sockaddr*)&caddr, &size)) == -1) error_handling("accept error");
    // 接受客户端请求信息
    // 请求头信息的缓存
    char header_buf[BUFSIZE];
    memset(header_buf, '\0', BUFSIZE);
    // 存放目标文件的缓存
    char *file_buf;
    // 文件信息
    struct stat file_stat;
    // 文件是否有效, 文件头信息缓存的长度
    int is_valid = 1, str_len = 0;
    // 判断文件的格式
    if (!(S_IROTH & file_stat.st_mode)) is_valid = 0;
    if (is_valid) {
        int len = sprintf(header_buf, BUFSIZE - 1, "%s %s\r\n", "HTTP/1.1");
        str_len += len;
        struct iovec iv[2];
        iv[0].iov_base = header_buf;
        iv[0].iov_len = strlen(header_buf);
        iv[1].iov_base = file_buf;
        iv[1].iov_len = file_stat.st_size;
        len = writev(cfd, iv, 2);
        
    }
    close(cfd);
    close(sfd);
    free(file_buf);
    return 0;
}

void error_handling(const char *msg) {
    perror(msg);
    exit(-1);
}