//
// Created by srdczk on 2019/11/26.
//

#ifndef SOCKET_UTIL_H
#define SOCKET_UTIL_H

#include <arpa/inet.h>
#include <ctime>
#define BUFSIZE 1024
class timer_node;
// client_data
class client_data {
public:
    sockaddr_in addr;
    int cfd;
    char buff[BUFSIZE];
    timer_node *timer;
};
class timer_node {
public:
    timer_node(): pre(nullptr), next(nullptr) {}
    timer_node *pre, *next;
    // 期望处理时间
    long expire;
    // 回调函数
    void (*callback)(client_data *);
    // 客户信息
    client_data *client;
};
// 升序链表
class timer_list {
public:
    timer_list();
    ~timer_list();
    void add_timer(timer_node *timer);
    void adjust_timer(timer_node *timer);
    void del_timer(timer_node *timer);
    void tick();
private:
    timer_node *head, *tail;
};
#endif //SOCKET_UTIL_H
