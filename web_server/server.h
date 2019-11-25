//
// Created by srdczk on 2019/11/25.
//

#ifndef SERVER_H
#define SERVER_H

void error_handling(const char *);
int init_listen_fd(int port, int epfd);
// epoll 在指定端口 运行
void epoll_run(int port);
// accept
void do_accept(int sfd, int epfd);
int get_line(int cfd, char *buff, int size);
void do_read(int cfd, int epfd);
void disconnect(int cfd, int epfd);
void http_request(const char *request, int cfd);
void send_respond_head(int cfd, int no, const char *desp, const char *type, long len);
void send_file(int cfd, const char *filename);
void send_dir(int cfd, const char *dirname);
const char *get_file_type(const char *name);
int set_nonblocking(int fd);
#endif //SERVER_H
