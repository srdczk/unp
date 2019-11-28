//
// Created by srdczk on 2019/11/28.
//

#ifndef HTTP_H
#define HTTP_H

#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#define PORT 9999
#define IP "127.0.0.1"
#define BUFSIZE 1024
int set_nonblocking(int fd) {
    int option = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, option | O_NONBLOCK);
    return option;
}
void add_fd(int epfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLET | EPOLLIN;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event) == -1) printf("NIAMSILE\n");
    set_nonblocking(fd);
}
// 每一次连接的实体
class http_connect {
public:
    http_connect() = default;
    void init(int, sockaddr_in *);
    bool read();
    void process();
    ~http_connect() = default;
    const char *get_name();
    static int m_epfd;
    void disconnect();
    int cfd;
private:
    sockaddr_in *caddr;
    char read_buff[BUFSIZE];
    char method[BUFSIZE], path[BUFSIZE], protocol[BUFSIZE];
    int get_line(char *buff, int size);
    const char *get_file_type(const char *name);
    void send_respond_head(int cfd, int no, const char *desp, const char *type, long len);
    void send_file(int cfd, const char *filename);
    void send_dir(int cfd, const char *name);
};

void http_connect::init(int c, sockaddr_in *ca) {
    cfd = c;
    caddr = ca;
}

bool http_connect::read() {
    char line[BUFSIZE];
    int len = get_line(line, BUFSIZE);
    if (!len) {
        printf("a client quit\n");
        disconnect();
    }
    while (len) {
        char buff[BUFSIZE];
        len = get_line(buff, sizeof(buff));
        printf("--->%s", buff);
    }
    if (!strncasecmp("get", line, 3)) {
        sscanf(line, "%[^ ] %[^ ] %[^ ]", method, path, protocol);
        return 1;
    }
    else return 0;
}


int http_connect::get_line(char *buff, int size) {
    char c = 'c';
    int sum = 0;
    while (sum < size - 1 && c != '\n') {
        int len = recv(cfd, &c, 1, 0);
        if (len > 0) {
            if (c == '\r') {
                int p = recv(cfd, &c, 1, MSG_PEEK);
                if (p > 0 && c == '\n') {
                    recv(cfd, &c, 1, 0);
                } else c = '\n';
            }
            buff[sum++] = c;
        } else c = '\n';
    }
    buff[sum] = '\0';
    return sum;
}
void http_connect::process() {
    char *file = path + 1;
    // 如果没有输入路径
    if (!strcmp(path, "/")) {
        file = "./";
    }
    struct stat st;
    // 在多线程环境中访问 (要不要加锁 ?) --> 读数据不用加锁
    int ret = stat(file, &st);
    if (ret == -1) {
        send_respond_head(cfd, 404, "File Not Found", ".html", -1);
        send_file(cfd, "404.html");
    }
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
// 通过文件名获取文件的类型
const char *http_connect::get_file_type(const char *name) {
    const char* dot = strrchr(name, '.');
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

// 发送响应的头信息 :
void http_connect::send_respond_head(int cfd, int no, const char *desp, const char *type, long len) {
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

void http_connect::send_file(int cfd, const char *filename) {
    char buff[BUFSIZE];
    int fd = open(filename, O_RDONLY), len;
    // 循环读取文件
    while (len = ::read(fd, buff, BUFSIZE)) {
        send(cfd, buff, len, 0);
    }
    close(fd);
}
void http_connect::send_dir(int cfd, const char *name) {
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

void http_connect::disconnect() {
    epoll_ctl(m_epfd, EPOLL_CTL_DEL, cfd, NULL);
    close(cfd);
}

int http_connect::m_epfd = -1;



#endif //HTTP_H
