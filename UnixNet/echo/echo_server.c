#include "echo_server.h"

int echo_server_run() {
    int sfd;
    if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        return error_handle("socket error");
    }
    // linux tcp dump to realize something
    struct sockaddr_in saddr;
    struct sockaddr_in caddr;
    socklen_t size = sizeof(caddr);
    memset(&saddr, '\0', sizeof(saddr));
    memset(&caddr, '\0', sizeof(caddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sfd, (struct sockaddr*)&saddr, sizeof(saddr)) == -1) {
        return error_handle("bind error");
    }

    if (listen(sfd, BUFFSIZE) == -1) {
        return error_handle("listen error");
    }

    int cfd = accept(sfd, (struct sockaddr *)&caddr, &size);
    if (cfd == -1) error_handle("accept error");
    printf("New connection from %s: %d\n", inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));
    char buff[BUFFSIZE];
    while (1) {
        ssize_t n = read(cfd, buff, BUFFSIZE);
        if (n <= 0) break;
        buff[n] = '\0';
        for (int i = 0; i < strlen(buff); ++i) {
            if (buff[i] >= 'a' && buff[i] <= 'z') {
                buff[i] = (char) (buff[i] - 'a' + 'A');
            }
        }
        n = write(cfd, buff, strlen(buff));
        if (n < 0) break;
    }
    close(cfd);
    close(sfd);
    return 0;
}