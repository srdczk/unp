//
// Created by srdczk on 2019/11/28.
//

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"
// 线程池, 队列上锁
template <class T>
class thread_pool {
public:
    thread_pool(int thread_num = 8, int max_requests = 10000);
    ~thread_pool();
    // 加一个请求
    bool append(T *request);
private:
    // 线程运行的函数
    static void *worker(void *arg);
    void run();
    int m_thread_num;
    int m_max_requests;
    pthread_t *m_threads;
    locker m_queuelock;
    sem m_queeustat;
    // 工作队列
    std::list<T *> queue;
    bool m_stop;
};
// 设置为
template <class T>
// 用 modern c++
thread_pool<T>::thread_pool(int thread_num, int max_requests) {
    m_thread_num = thread_num;
    m_max_requests = max_requests;
    m_stop = 0;
    if (thread_num > 0) m_threads = new pthread_t[thread_num];
    else m_threads = NULL;
    for (int i = 0; i < thread_num; ++i) {
        if (pthread_create(m_threads + i, NULL, worker, this)) {
            delete [] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i])) {
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template <class T>
thread_pool<T>::~thread_pool() {
    delete [] m_threads;
    m_stop = 1;
}
// run 和append 操作 --> p v 生产者消费者模式
template <class T>
bool thread_pool<T>::append(T *request) {
    m_queuelock.lock();
    if (queue.size() > m_max_requests) {
        m_queuelock.unlock();
        return 0;
    }
    queue.push_back(request);
    m_queuelock.unlock();
    m_queeustat.post();
    return true;
}

template <class T>
void *thread_pool<T>::worker(void *arg) {
    thread_pool *pool = (thread_pool*) arg;
    pool->run();
    return pool;
}
// 生产者, 消费者模式
template <class T>
void thread_pool<T>::run() {
    // 从队列中消耗
    while (!m_stop) {
        // 先上锁
        m_queeustat.wait();
        m_queuelock.lock();
        if (!queue.size()) {
            m_queuelock.unlock();
            continue;
        }
        T *request = queue.front();
        printf("queue: size:%d\n", queue.size());
        queue.pop_front();
        m_queuelock.unlock();
        if (!request) continue;
        request->process();
    }
}
#endif //THREADPOOL_H
