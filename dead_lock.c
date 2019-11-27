#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
pthread_mutex_t a, b;
// 死锁演示

void *thread(void *arg) {
    pthread_mutex_lock(&b);
    printf("thread Get B\n");
    sleep(5);
    printf("thread Try to get A\n");
    pthread_mutex_lock(&a);
    printf("thread Get A\n");
    pthread_mutex_unlock(&a);
    pthread_mutex_unlock(&b);
    return NULL;
}

int main() {
    pthread_t id;
    // 置为 0
    pthread_mutex_init(&a, NULL), pthread_mutex_init(&b, NULL);
    pthread_create(&id, NULL, thread, NULL);
    pthread_mutex_lock(&a);
    printf("main Get A\n");
    sleep(5);
    printf("main Try to get B\n");
    pthread_mutex_lock(&b);
    printf("main Get B\n");
    pthread_mutex_unlock(&b);
    pthread_mutex_unlock(&a);
    pthread_join(id, NULL);
    pthread_mutex_destroy(&a), pthread_mutex_destroy(&b);
    return 0;
}