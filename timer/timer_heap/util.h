//
// Created by srdczk on 2019/11/26.
//

#ifndef SOCKET_UTIL_H
#define SOCKET_UTIL_H

#include <arpa/inet.h>
#include <ctime>
#define MAX_SIZE 2000
#define BUFSIZE 1024
// 也要有一个指针, 指向 heap 中的值
class client_data {
public:
    sockaddr_in addr;
    int cfd;
    char buff[BUFSIZE];
    // 时间堆中的指针
    int index;
};

class time_node {
public:
    long expire;
    // 回调函数指针
    void (*callback)(client_data *);
    // 指向的客户
    client_data *data;
};

class time_heap {
public:
    // insert 返回下标
    int insert(time_node &node);
    // adjust 返回下标
    int adjust(int index, long amount);
    // 直接删除
    void del(int index);
    void tick();
private:
    // 时间最大堆子
    int max_size = MAX_SIZE;
    int size = 0;
    // 最大
    time_node a[MAX_SIZE];
    int heapify(int index);
};

#endif //SOCKET_UTIL_H
