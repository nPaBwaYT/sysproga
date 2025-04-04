#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#define MSG_SIZE 256
#define PROJECT_ID 3345

typedef struct
{
    long msg_type;
    char msg_text[MSG_SIZE];
    key_t client_queue_key;
} message;

void list_files(char *path)
{
    struct dirent *entry;
    DIR *dir = opendir(path);

    if (dir == NULL)
    {
        return;
    }
    printf("  filename             inode\n");
    while ((entry = readdir(dir)) != NULL)
    {

        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
        {
            continue;
        }
        if (entry->d_type == DT_REG)
        {
            printf("  \033[34m%-20s\033[0m %-8lu\n", entry->d_name, entry->d_ino);
        }
        else if (entry->d_type == DT_DIR)
        {
            printf("  \033[32m%-20s\033[0m  %-8lu\n", entry->d_name, entry->d_ino);
        }
        else
        {
            printf("  \033[33m%-20s\033[0m %-8lu\n", entry->d_name, entry->d_ino);
        }
    }

    closedir(dir);
}


void *handle(void *args) {

    int *kill = ((int **)args)[0];
    int *exit = ((int **)args)[1];

    while (!(*exit)) {
        char *line = NULL;
        size_t len = 0;
        getline(&line, &len, stdin);

        if (strcmp(line, "kill\n") == 0) {
            *kill = 1;
        } else if (strcmp(line, "exit\n") == 0) {
            *exit = 1;
        }

        free(line);
    }
    return NULL;
}


int main()
{
    int kill = 0;
    int exit = 0;
    pthread_t handler;

    int *args[2];
    args[0] = &kill;
    args[1] = &exit;

    pthread_create(&handler, NULL, handle, (void *)args);

    while (!exit) {

        key_t key;
        int msgid;
        message msg;

        key = ftok("/tmp", PROJECT_ID);
        if (key == -1)
        {
            return -1;
        }

        msgid = msgget(key, IPC_CREAT | 0666);
        if (msgid == -1)
        {

            return -1;
        }

        printf("Сервер готов к приему сообщений...\n");
        kill = 0;

        while (!kill && !exit) {
            
            if (msgrcv(msgid, &msg, sizeof(message) - sizeof(long), 0, IPC_NOWAIT) == -1) {
                continue;
            }

            if (!strcmp(msg.msg_text, "PING"))
            {
                int client_msgid = msgget(msg.client_queue_key, 0666);
                if (client_msgid == -1)
                {
                    continue;
                }
                message pong_msg = {
                    .msg_type = 2,
                };
                strcpy(pong_msg.msg_text, "PONG");
                if (msgsnd(client_msgid, &pong_msg, sizeof(pong_msg) - sizeof(long), 0) == -1)
                {
                    continue;
                }
                else
                {
                    printf("Отправлен PONG в очередь %d\n", msg.client_queue_key);
                }
                continue;
            }

            if (strcmp(msg.msg_text, "exit") == 0)
            {
                continue;
            }
            else
            {
                printf("Получен запрос на просмотр каталога: %s\n", msg.msg_text);
                list_files(msg.msg_text);
            }
        }

        if (msgctl(msgid, IPC_RMID, NULL) == -1)
        {
            return -1;
        }

        printf("Сервер завершил работу.\n");
    }
    return 0;
}
