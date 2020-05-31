#ifndef UNIXNET_GLOBAL_H
#define UNIXNET_GLOBAL_H

#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int error_handle(const char *s);

#define LOCAL_HOST "127.0.0.1"
#define PORT 8888
#define BUFFSIZE 1024

#endif //UNIXNET_GLOBAL_H
