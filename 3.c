#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

#define N 5
#define ITER_COUNT 15

void sem_decreace(int semid, int semnum)
{
    struct sembuf op = {semnum, -1, SEM_UNDO};
    semop(semid, &op, 1);
}

void sem_increace(int semid, int semnum)
{
    struct sembuf op = {semnum, 1, SEM_UNDO};
    semop(semid, &op, 1);
}

void philosopher(int id, int semid)
{
    int left = id;
    int right = (id + 1) % N;
    for (int i = 0; i < ITER_COUNT; i++)
    {
        printf("Философ %d думает\n", id);
        sleep(rand() % 3 + 1);

        printf("Философ %d хочет поесть\n", id);

        if (id % 2 == 0)
        {
            sem_decreace(semid, left);
            sem_decreace(semid, right);
        }
        else
        {
            sem_decreace(semid, right);
            sem_decreace(semid, left);
        }

        printf("Философ %d ест\n", id);
        sleep(rand() % 2 + 1);

        sem_increace(semid, left);
        sem_increace(semid, right);

        printf("Философ %d закончил есть\n", id);
        i++;
    }
}

int main()
{
    srand(time(0));
    int semid = semget(IPC_PRIVATE, N, IPC_CREAT | 0666);
    if (semid == -1)
    {
        printf("Ошибка получения id набора семафоров\n");
        return -1;
    }
    union semun
    {
        int val;
    } arg;
    for (int i = 0; i < N; i++)
    {
        arg.val = 1;
        semctl(semid, i, SETVAL, arg);
    }

    for (int i = 0; i < N; i++)
    {   
        if (fork() == 0)
        {
            philosopher(i, semid);
            return 0;
        }
    }

    sleep(10);
    semctl(semid, 0, IPC_RMID);
    printf("-----СЕМАФОРЫ ОТВЯЗАНЫ-----\n");
    for (int i = 0; i < N; ++i) {
        wait(NULL);
    }

    return 0;
}
