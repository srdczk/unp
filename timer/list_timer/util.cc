//
// Created by srdczk on 2019/11/26.
//
#include "util.h"

timer_list::timer_list() {
    head = new timer_node, tail = new timer_node;
    head->next = tail;
    tail->pre = head;
}
timer_list::~timer_list() {
    timer_node *node = head;
    while (node) {
        timer_node *next = node->next;
        delete node;
        node = next;
    }
}
void timer_list::add_timer(timer_node *timer) {
    if (!timer) return;
    timer_node *node = head->next;
    while (node != tail && node->expire < timer->expire) node = node->next;
    // 插入 node 之前
    timer_node *pre = node->pre;
    timer->pre = pre;
    timer->next = node;
    pre->next = timer;
    node->pre = timer;
}

void timer_list::del_timer(timer_node *timer) {
    if (!timer) return;
    timer_node *next = timer->next, *pre = timer->pre;
    next->pre = pre;
    pre->next = next;
    // 直接在这删除
    delete timer;
}
void timer_list::adjust_timer(timer_node *timer) {
    if (!timer) return;
    timer_node *next = timer->next, *pre = timer->pre;
    next->pre = pre;
    pre->next = next;
    while (next != tail && next->expire < timer->expire) next = next->next;
    pre = next->pre;
    timer->pre = pre;
    pre->next = timer;
    next->pre = timer;
    timer->next = next;
}

void timer_list::tick() {
    // 每次信号触发之时调用该函数, 进行超时事件的回调函数
    // 现在的时间
    time_t cur = time(nullptr);
    timer_node *node = head->next;
    while (node != tail) {
        if (node->expire > cur) break;
        timer_node *next = node->next;
        // 回调函数处理
        node->callback(node->client);
        del_timer(node);
        node = next;
    }
}
