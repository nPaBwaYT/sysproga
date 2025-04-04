#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define SERVER_KEY 1234
#define CLIENT_KEY 12345
#define COMMAND_SIZ 8
#define MSG_SIZ 128

typedef struct
{
    long mtype;
    long client_id;            // client_id
    char command[COMMAND_SIZ]; // Команда (take, put, move)
    char object[COMMAND_SIZ];  // Объект (wolf, goat, cabbage)
    int status_code;
    char msg[MSG_SIZ]; // Описание
    key_t client_queue_key;
} msg_buffer;

int main(int agrc, char *argv[])
{
    if (agrc < 2) {
        printf("Используйте: ./client <file>\n");
        return -1;
    }

    FILE *file = fopen(argv[1], "r");
    if (!file) {
        return -1;
    }

    key_t key = ftok("/tmp", SERVER_KEY);
    if (key == -1) {
        return -1;
    }
    key_t client_key = CLIENT_KEY + getpid();

    int client_msgid = msgget(client_key, IPC_CREAT | 0666);
    if (client_msgid == -1) {
        return -1;
    }

    int server_msgid = msgget(key, IPC_CREAT | 0666);
    if (server_msgid == -1) {
        msgctl(client_msgid, IPC_RMID, NULL);
        return -1;
    }

    msg_buffer message;

    message.mtype = 1;
    strcpy(message.msg, "PING");
    message.client_queue_key = client_key;
    message.client_id = getpid();

    printf("Отправка PING\n");
    if (msgsnd(server_msgid, &message, sizeof(message) - sizeof(long), 0) == -1) {
        msgctl(client_msgid, IPC_RMID, NULL);
        return -1;
    }

    int attempts = 5;

    while (attempts--) {
        if (msgrcv(client_msgid, &message, sizeof(message) - sizeof(long), 2, IPC_NOWAIT) != -1)
        {
            printf("Получен PONG: %s\n", message.msg);
            msgctl(client_msgid, IPC_RMID, NULL);
            break;
        }
        sleep(1);
    }

    if (attempts == -1) {
        printf("Сервер не ответил.\n");
        msgctl(client_msgid, IPC_RMID, NULL);
        return -1;
    }

    while (1) {
        char command[10];
        char object[10];
        memset(object, 0, sizeof(object));
        if (fscanf(file, "%s", command) == EOF)
        {
            return 0;
        }
        if (!strcmp(command, "take"))
        {
            fscanf(file, "%s", object);
        }

        printf("%s %s\n", command, object);
        message.mtype = getpid();
        message.client_id = getpid();
        strcpy(message.object, object);
        strcpy(message.command, command);

        if (msgsnd(server_msgid, &message, sizeof(msg_buffer) - sizeof(message.mtype), 0) == -1)
        {
            return -1;
        }
        if (msgrcv(server_msgid, &message, sizeof(msg_buffer) - sizeof(message.mtype), message.mtype, 0) == -1)
        {
            return -1;
        }
        if (message.status_code == -1)
        {
            printf("Ответ сервера: %s", message.msg);
            printf("Завершение работы\n");
            return -1;
        }
        printf("Ответ сервера: %s\n", message.msg);
    }
    fclose(file);

    return 0;
}