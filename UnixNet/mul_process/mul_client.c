#include "mul_client.h"

void read_from(int fd, char *buff) {
    while (1) {
        ssize_t n = read(fd, buff, BUFFSIZE);
        if (n > 0) {
            buff[n] = '\0';
            printf("Recv:%s\n", buff);
        } else break;
    }
    printf("Read End\n");
}

void write_to(int fd, char *buff) {
    while (1) {
        printf("Please Input:\n");
        scanf("%s", buff);
        ssize_t n = write(fd, buff, strlen(buff));
        if (n <= 0) break;
    }
    printf("Write End\n");
}

int multi_process_client_run() {

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
    if (!fork()) {
        read_from(cfd, buff);
    } else {
        write_to(cfd, buff);
    }

    return 0;
}


