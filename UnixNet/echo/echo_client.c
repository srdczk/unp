#include "echo_client.h"


int echo_client_run() {
    int cfd;
    if ((cfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        return error_handle("socket error");
    }
    struct sockaddr_in caddr;
    memset(&caddr, '\0', sizeof(caddr));
    caddr.sin_family = AF_INET;
    caddr.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    caddr.sin_port = htons(PORT);
    if (connect(cfd, (struct sockaddr*)&caddr, sizeof(caddr)) == -1) {
        return error_handle("connect error");
    }
    char buff[BUFFSIZE];
    while (1) {
        printf("Please input:\n");
        scanf("%s", buff);
        if (!strcmp(buff, "Q\n")) break;
        ssize_t n;
        if ((n = write(cfd, buff, strlen(buff))) < 0) {
            return error_handle("write error");
        }
        n = read(cfd, buff, BUFFSIZE);
        if (n > 0) buff[n] = '\0';
        printf("Recv:%s\n", buff);
    }
    close(cfd);
    return 0;
}
