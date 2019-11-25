#include "server.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>

#define EPOLL_SIZE 2000

#define BUFSIZE 1024
void epoll_run(int port) {
    int epfd, sfd;
    if ((epfd = epoll_create(EPOLL_SIZE)) == -1) error_handling("epoll create error");
    if ((sfd = init_listen_fd(port, epfd)) == -1) error_handling("init sfd error");
    // 所有节点数组 :
    struct epoll_event events[EPOLL_SIZE];
    while (1) {
        int num = epoll_wait(epfd, events, EPOLL_SIZE, -1);
        if (num < 0) break;
        for (int i = 0; i < num; ++i) {
            // 只处理EPOLLIN
            struct epoll_event *ptr = &events[i];
            if (!(ptr->events & EPOLLIN)) continue;
            else if (ptr->data.fd == sfd) {
                do_accept(sfd, epfd);
            } else {
                printf("NIMAISLE\n");
                do_read(ptr->data.fd, epfd);
            }
        }
    }
}
void do_read(int cfd, int epfd) {
    char line[BUFSIZE];
    int len = get_line(cfd, line, sizeof(line));
    if (!len) {
        printf("a client quit\n");
        disconnect(cfd, epfd);
    }
    while (len) {
        char buff[BUFSIZE];
        len = get_line(cfd, buff, sizeof(buff));
        printf("--->%s", buff);
    }
    if (!strncasecmp("get", line, 3)) {
        http_request(line, cfd);
        disconnect(cfd, epfd);
        // 关闭连接
    }
}

// 发送响应的头信息 :
void send_respond_head(int cfd, int no, const char *desp, const char *type, long len) {
    // 向 cfd 发送. no 表示状态码. desp 描述, type -->
    char buff[1024] = {'\0'};
    sprintf(buff, "HTTP/1.1 %d %s\r\n", no, desp);
    send(cfd, buff, strlen(buff), 0);
    sprintf(buff, "Content-Type:%s\r\n", type);
    sprintf(buff + strlen(buff), "Content-Length:%ld\r\n", len);
    send(cfd, buff, strlen(buff), 0);
    // 协议规定 --> 一定要有空行
    send(cfd, "\r\n", 2, 0);
}

void send_file(int cfd, const char *filename) {
    char buff[BUFSIZE];
    int fd = open(filename, O_RDONLY), len;
    // 循环读取文件
    while (len = read(fd, buff, BUFSIZE)) {
        send(cfd, buff, len, 0);
    }
    close(fd);
}

// 处理请求函数
void http_request(const char *line, int cfd) {
    // 提取请求的路径, 分析是文件还是文件夹, 或者不存在, 给出响应的response;
    char method[12], path[1024], protocol[12];
    // 正则表达式
    sscanf(line, "%[^ ] %[^ ] %[^ ]", method, path, protocol);
    printf("method:%s path:%s protocol:%s\n", method, path, protocol);
    char *file = path + 1;
    // 如果没有输入路径
    if (!strcmp(path, "/")) {
        file = "./";
    }
    struct stat st;
    int ret = stat(file, &st);
    // 判断文件的属性
    // 如果, 不存在该文件
    if (ret == -1) {
        // 发送 404 的页面
        send_respond_head(cfd, 404, "File Not Found", ".html", -1);
        send_file(cfd, "404.html");
    }
    // 如果是文件夹
    if (S_ISDIR(st.st_mode)) {
        // 不要Content-Length
        send_respond_head(cfd, 200, "OK", get_file_type(".html"), -1);
        send_dir(cfd, file);
    } else if (S_ISREG(st.st_mode)) {
        // 如果是文件
        // 查找文件的信息
        send_respond_head(cfd, 200, "OK", get_file_type(file), st.st_size);
        send_file(cfd, file);
    }
}

void send_dir(int cfd, const char *name) {
    // 便利目录, 拼成html
    char buff[1024];
    memset(buff, '\0', sizeof(buff));
    sprintf(buff, "<html><head><title>DIR: %s</title></head>", name);
    sprintf(buff + strlen(buff), "<body><h1>Current Dir: %s</h1><table>", name);
    // 文件二级指针
    char path[1024] = {'\0'};
    struct dirent** ptr;
    int num = scandir(name, &ptr, NULL, alphasort);
    for (int i = 0; i < num; ++i) {
        char *cname = ptr[i]->d_name;
        if (strlen(name) > 0 && name[strlen(name) - 1] == '/') sprintf(path, "%s%s", name, cname);
        else sprintf(path, "%s/%s", name, cname);

        sprintf(buff + strlen(buff),
                "<tr><td><a href=\"%s\">%s</a></td></tr>",
                path, cname);
        send(cfd, buff, strlen(buff), 0);
        memset(buff, '\0', sizeof(buff));
    }
    sprintf(buff + strlen(buff), "</table></body></html>");
    send(cfd, buff, strlen(buff), 0);
}


// 处理连接函数
void do_accept(int sfd, int epfd) {
    // 新建立连接
    int cfd;
    struct sockaddr_in addr;
    socklen_t size = sizeof(addr);
    if ((cfd = accept(sfd, (struct sockaddr*)&addr, &size)) == -1) error_handling("accept error");
    // 添加到epoll 事件树上
    // 设置非阻塞, + ET 边缘触发模式, 实现长连接
    char ip[64];
    memset(ip, '\0', sizeof(ip));
    printf("A new client: %s: %d\n", inet_ntop(AF_INET, &addr.sin_addr.s_addr, ip, sizeof(ip)), ntohs(addr.sin_port));
    set_nonblocking(cfd);
    struct epoll_event event;
    event.data.fd = cfd;
    event.events = EPOLLIN | EPOLLET;
    // 添加到其上
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &event) == -1) error_handling("epoll add error");
}

int init_listen_fd(int port, int epfd) {
    int sfd;
    struct sockaddr_in addr;
    if ((sfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) return -1;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    // 接着bind + listen
    // 端口复用
    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(opt));

    if ((bind(sfd, (struct sockaddr*)&addr, sizeof(addr))) == -1) return -1;
    if (listen(sfd, 64) == -1) return -1;
    // 添加到 epoll (红黑树)上
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = sfd;
    if ((epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &event)) == -1) return -1;
    return sfd;
}

int set_nonblocking(int fd) {
    int option = fcntl(fd, F_GETFL);
    int new_option = option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return option;
}

void disconnect(int cfd, int epfd) {
    // 注意epoll 移除的时候不用 event对象
    if (epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL) == -1) error_handling("epoll del error");
    close(cfd);
}

// 通过文件名获取文件的类型
const char *get_file_type(const char *name) {
    char* dot;
    // 自右向左查找‘.’字符, 如不存在返回NULL
    dot = strrchr(name, '.');
    if (dot == NULL)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp( dot, ".wav" ) == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}

void error_handling(const char *msg) {
    perror(msg);
    exit(-1);
}

int get_line(int cfd, char *buff, int size) {
    int sum = 0;
    char c = '\0';
    while (sum < size - 1 && c != '\n') {
        int len = recv(cfd, &c, 1, 0);
        if (len > 0) {
            if (c == '\r') {
                // 提前看一个, 不取出来
                len = recv(cfd, &c, 1, MSG_PEEK);
                if (len > 0 && c == '\n') {
                    recv(cfd, &c, 1, 0);
                } else {
                    c = '\n';
                }
            }
            buff[sum++] = c;
        } else {
            c = '\n';
        }
    }
    buff[sum] = '\0';
    return sum;
}