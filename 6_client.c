#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>
#include <unistd.h>

#define MSG_SIZE 256
#define CLIENT_KEY 12345
#define PROJECT_ID 3345

typedef struct {
    long msg_type;
    char msg_text[MSG_SIZE];
    key_t client_queue_key;
} message;

int main() {
    while (1) {
        key_t key;
        message msg;

        key = ftok("/tmp", PROJECT_ID);
        if (key == -1) {
            printf("Ошибка получения ключа\n");
            return -1;
        }

        key_t client_key = CLIENT_KEY + getpid();

        int client_msgid = msgget(client_key, IPC_CREAT | 0666);
        if (client_msgid == -1) {
            printf("Ошибка подключения/создания очереди клиента\n");
            return -1;
        }

        int msgid = msgget(key, 0666);
        if (msgid == -1) {
            printf("Ошибка подключения/создания очереди сервера\n");
            return -1;
        }

        msg.msg_type = 1;
        strcpy(msg.msg_text, "PING");
        msg.client_queue_key = client_key;

        printf("Отправка PING\n");
        if (msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
            msgctl(client_msgid, IPC_RMID, NULL);
            return -1;
        }

        int attempts = 5;

        while (attempts--) {
            if (msgrcv(client_msgid, &msg, sizeof(msg) - sizeof(long), 2, IPC_NOWAIT) != -1) {
                printf("Получен PONG: %s\n", msg.msg_text);
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

        printf("Введите сообщение (или 'exit' для выхода):\n");

        while (1) {

            fgets(msg.msg_text, MSG_SIZE, stdin);
            msg.msg_text[strcspn(msg.msg_text, "\n")] = 0;
            msg.msg_type = 1;

            if (msgsnd(msgid, &msg, sizeof(msg.msg_text), 0) == -1) {
                printf("Переподключение...\n");
                break;
            }

            if (strncmp(msg.msg_text, "exit", 4) == 0) {
                printf("Клиент завершил работу.\n");
                return 0;
            }
        }    
    }
}
