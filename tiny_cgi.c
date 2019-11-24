#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

void error_handling(const char *);

int main(int ac, const char **av) {
    if (ac < 3) error_handling("input error");
    const char *ip = av[1];
    int port = atoi(av[2]);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    // ipv4
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    // p to n
    inet_pton(AF_INET, ip, &addr.sin_addr);
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock != -1);

    int ret = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);

    ret = listen(sock, 5);
    assert(ret != -1);

    struct sockaddr_in client;
    socklen_t size = sizeof(client);
    int clnt_fd = accept(sock, (struct sockaddr*)&client, &size);
    if (clnt_fd < 0) error_handling("connect error");
    else {
        close(STDOUT_FILENO);
        dup(clnt_fd);
        printf("abcd\n");
        close(clnt_fd);
    }
    close(sock);

    return 0;
}

void error_handling(const char *msg) {
    perror(msg);
    exit(-1);
}