//
// Created by srdczk on 2019/11/26.
//
#include <iostream>
#include "util.h"

int time_heap::heapify(int index) {
    int left = 2 * index + 1;
    while (left < size) {
        int smallest = left + 1 < size && a[left + 1].expire < a[left].expire ? left + 1 : left;
        smallest = a[index].expire < a[smallest].expire ? index : smallest;
        if (index == smallest) break;
        std::swap(a[index], a[smallest]);
        index = smallest;
        left = 2 * index + 1;
    }
    return index;
}

int time_heap::insert(time_node &node) {
    a[size++] = node;
    int t = size - 1;
    while ((a[(t - 1) / 2].expire > a[t].expire)) {
        std::swap(a[(t - 1) / 2], a[t]);
        t = (t - 1) / 2;
    }
    return t;
}

int time_heap::adjust(int index, long amout) {
    a[index].expire = amout;
    return heapify(index);
}

void time_heap::del(int index) {
    std::swap(a[index], a[--size]);
    heapify(index);
}

void time_heap::tick() {
    time_t cur = time(nullptr);
    while (size > 0 && a[0].expire < cur) {
        a[0].callback(a[0].data);
        del(0);
    }
}