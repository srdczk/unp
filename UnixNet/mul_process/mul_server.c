#include "mul_server.h"


void child_handler(int sig) {
    // call wait pid to wait child process
    pid_t pid;
    int status;
    pid = waitpid(-1, &status, WNOHANG);
    printf("remove pid:%d\n", pid);
}

int multi_process_server_run() {
    int sfd;
    // stack also copy
    struct sockaddr_in saddr, caddr;
    socklen_t size = sizeof(caddr);
    memset(&saddr, '\0', sizeof(saddr));

    if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) error_handle("socket error");
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);
    if (bind(sfd, (struct sockaddr*)&saddr, sizeof(saddr)) == -1) error_handle("bind error");
    if (listen(sfd, 1024) == -1) error_handle("listen error");
    struct sigaction sa;
    sa.sa_handler = child_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGCHLD, &sa, 0);
    char buff[BUFFSIZE];
    while (1) {
        // accept
        memset(&caddr, '\0', sizeof(caddr));
        size = sizeof(caddr);
        int cfd = accept(sfd, (struct sockaddr*)(&caddr), &size);
        if (cfd < 0) continue;
        printf("New connection from %s: %d\n", inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));
        if (!fork()) {
            close(sfd);
            while (1) {
                int n = read(cfd, buff, BUFFSIZE);
                if (n <= 0) break;
                buff[n] = '\0';
                for (int i = 0; i < n; ++i) {
                    if (buff[i] >= 'a' && buff[i] <= 'z')
                        buff[i] = buff[i] - 'a' + 'A';
                }
                n = write(cfd, buff, strlen(buff));
            }
            close(cfd);
            printf("Remove from this!");
            return 0;
        } else close(cfd);
    }
    close(sfd);
    return 0;
}