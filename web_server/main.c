#include "server.h"
#define PORT 9999
int main() {
    // 实现一个webserver
    epoll_run(PORT);
    return 0;
}