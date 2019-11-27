//
// Created by srdczk on 2019/11/27.
//

// 线程相关类 c++ 封装
#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>
// 信号量
class sem {
public:
    sem() {
        if (sem_init(&m_sem, 0, 0)) throw std::exception();
    }
    // 析构函数
    ~sem() {
        sem_destroy(&m_sem);
    }
    bool wait() {
        return !sem_wait(&m_sem);
    }
    bool post() {
        return !sem_post(&m_sem);
    }
private:
    sem_t m_sem;
};

class locker {
public:
    locker() {
        if (pthread_mutex_init(&m_mutex, NULL)) throw std::exception();
    }
    ~locker() {
        pthread_mutex_destroy(&m_mutex);
    }
    // 锁
    bool lock() {
        return !pthread_mutex_lock(&m_mutex);
    }
    bool unlock() {
        return !pthread_mutex_unlock(&m_mutex);
    }
private:
    pthread_mutex_t m_mutex;
};

// 条件变量, 依附于互斥量
class cond {
public:
    cond() {
        if (pthread_mutex_init(&m_mutex, nullptr)) throw std::exception();
        if (pthread_cond_init(&m_cond, nullptr)) throw std::exception();
    }
    ~cond() {
        pthread_cond_destroy(&m_cond);
        pthread_mutex_destroy(&m_mutex);
    }
    bool wait() {
        return !pthread_cond_wait(&m_cond, &m_mutex);
    }
    bool signal() {
        return !pthread_cond_signal(&m_cond);
    }
private:
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
};

#endif //LOCKER_H
