#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define HUMAN_COUNT 10
#define ITER_COUNT 2

int state = 0; // Состояние ванны (0: пусто, 1: женщины, 2: мужчины)
int woman_count = 0;
int man_count = 0;
int N;
pthread_mutex_t mutex;
sem_t semaphore;

void *woman_wants_to_enter()
{
    pthread_mutex_lock(&mutex);

    while (state == 2 || (state == 1 && woman_count >= N))
    {
        printf("Woman is waiting...\n");
        pthread_mutex_unlock(&mutex);
        sleep(1);
        pthread_mutex_lock(&mutex);
    }

    woman_count++;
    state = 1;
    pthread_mutex_unlock(&mutex);

    
    printf("Woman entered the bath.\n");

    return NULL;
}

void *man_wants_to_enter()
{
    pthread_mutex_lock(&mutex);

    while (state == 1 || (state == 2 && man_count >= N))
    {
        printf("Man is waiting...\n");
        pthread_mutex_unlock(&mutex);
        sleep(1);
        pthread_mutex_lock(&mutex);
    }

    man_count++;
    state = 2;
    pthread_mutex_unlock(&mutex);

    
    printf("Man entered the bath.\n");

    return NULL;
}

void *woman_leaves()
{
    pthread_mutex_lock(&mutex);

    woman_count--;
    if (woman_count == 0)
    {
        state = 0;
    }

    pthread_mutex_unlock(&mutex);

    
    printf("Woman exited the bath.\n");

    return NULL;
}

void *man_leaves()
{
    pthread_mutex_lock(&mutex);

    man_count--;
    if (man_count == 0)
    {
        state = 0;
    }

    pthread_mutex_unlock(&mutex);

    
    printf("Man exited the bath.\n");

    return NULL;
}

void *simulate_woman()
{
    for (int i = 0; i < ITER_COUNT; ++i)
    {
        woman_wants_to_enter(NULL);
        sleep(rand() % 3 + 2);
        woman_leaves(NULL);
        sleep(rand() % 3 + 5);
    }
    return NULL;
}

void *simulate_man()
{
    for (int i = 0; i < ITER_COUNT; ++i)
    {
        man_wants_to_enter(NULL);
        sleep(rand() % 3 + 3);
        man_leaves(NULL);
        sleep(rand() % 3 + 5);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    srand(time(0));
    if (argc != 2)
    {
        printf("usage: %s <max people count>\n", argv[0]);
        return 1;
    }

    N = atoi(argv[1]);
    if (N <= 0)
    {
        printf("max people count must be positive.\n");
        return 1;
    }

    pthread_mutex_init(&mutex, NULL);
    

    pthread_t threads[HUMAN_COUNT];
    int thread_count = 0;

    for (int i = 0; i < 5; ++i)
    {
        if (pthread_create(&threads[thread_count++], NULL, simulate_woman, NULL) != 0)
        {
            printf("thread error\n");
            continue;
        }
        if (pthread_create(&threads[thread_count++], NULL, simulate_man, NULL) != 0)
        {
            printf("thread error\n");
            continue;
        }
    }

    for (int i = 0; i < thread_count; ++i)
    {
        if (pthread_join(threads[i], NULL) != 0)
        {
            perror("thread error");
        }
    }

    pthread_mutex_destroy(&mutex);

    return 0;
}