#include <sys/sem.h>
// 信号量
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
// 联合体
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short int *array;
    struct seminfo *__buf;
};
// 信号量的pv操作, -1 p, 1 v
void pv(int sem_id, int op) {
    struct sembuf sem_b;
    sem_b.sem_num = 0;
    sem_b.sem_flg = SEM_UNDO;
    sem_b.sem_op = op;
    semop(sem_id, &sem_b,1);
}

int main() {
    // 创建一个新的信号量
    int sem_id1 = semget(IPC_PRIVATE, 1, 0666), sem_id2 = semget(IPC_PRIVATE, 1, 0666), n;
    // 创建一个初始值为 1 , 一个初始值为 0 的信号量
    union semun sem_un1, sem_un2;
    sem_un1.val = 1, sem_un2.val = 0;
    // 设置子进程, 父进程交替输出
    semctl(sem_id1, 0, SETVAL, sem_un1);
    semctl(sem_id2, 0, SETVAL, sem_un2);
    // 设置信号量的初始值 1
    scanf("%d", &n);
    pid_t pid = fork();
    if (!pid) {
        // 子进程
        for (int i = 0; i < n; ++i) {
            pv(sem_id1, -1);
            printf("I'm child\n");
            pv(sem_id2, 1);
        }
    } else if (pid > 0) {
        for (int i = 0; i < n; ++i) {
            pv(sem_id2, -1);
            printf("I'm father\n");
            pv(sem_id1, 1);
        }
    } else {
        printf("fork error\n");
        exit(0);
    }
    // 等待子进程 结束, 防止僵尸进程
    waitpid(pid, NULL, 0);
    // 删除信号量
    semctl(sem_id1, 0, IPC_RMID, sem_un1);
    semctl(sem_id2, 0, IPC_RMID, sem_un2);
    return 0;
}