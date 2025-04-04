#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>


#define TABLE_SIZ 128
#define PROJECT_ID 1234
#define COMMAND_SIZ 8
#define MSG_SIZ 128


typedef struct {
    long mtype;
    long client_id;            
    char command[COMMAND_SIZ]; 
    char object[COMMAND_SIZ];  
    int status_code;
    char msg[MSG_SIZ]; 
    key_t client_queue_key;
} msg_buffer;


typedef struct {
    int peasant_side; 
    int boat_has;     
    int wolf_side;    
    int goat_side;    
    int cabbage_side; 
} UserState;


typedef struct Note {
    long client_id;
    UserState *state;
    struct Note *next;
} Note;


typedef struct {
    Note **notes;
    int size;
    int cap;
} HashTable;


HashTable *create_table() {

    HashTable *table = (HashTable *)malloc(sizeof(HashTable));
    if (table == NULL) {
        return NULL;
    } table->notes = (Note **)calloc(TABLE_SIZ, sizeof(Note *));
    if (table->notes == NULL) {
        free(table);
        return NULL;
    } table->cap = TABLE_SIZ;
    table->size = 0;
    return table;
}

void free_table(HashTable *t) {
    if (!t || !t->notes) {
        return;
    }

    for (int i = 0; i < t->cap; i++) {
        Note *current = t->notes[i];
        while (current) {
            Note *temp = current;
            current = current->next;
            free(temp->state);
            free(temp);
        } } free(t->notes);
    free(t);
}

int get_hash(HashTable *t, long client_id) {
    return client_id % t->cap;
}

Note *find_note(HashTable *t, long client_id) {
    int index = get_hash(t, client_id);
    Note *note = t->notes[index];

    while (note != NULL) {
        if (note->client_id == client_id) {
            return note;
        }     note = note->next;
    }

    return NULL;
}

UserState *get_user_state(msg_buffer *msg, HashTable *t) {
    Note *client = find_note(t, msg->client_id);
    if (!client) {
        return NULL;
    } return client->state;
}

void reset_user_state(UserState *state) { 
    state->boat_has = 0;
    state->peasant_side = 0; 
    state->wolf_side = 0;
    state->goat_side = 0;
    state->cabbage_side = 0;
}

void resize_table(HashTable *t) {
    int old_capacity = t->cap;
    int new_capacity = old_capacity * 2;

    Note **new_notes = (Note **)calloc(new_capacity, sizeof(Note *));
    if (!new_notes) {
        printf("Ошибка выделения памяти при увеличении таблицы\n");
        return;
    }

    for (int i = 0; i < old_capacity; i++) {
        Note *note = t->notes[i];
        while (note) {
            Note *next = note->next;
            int new_index = get_hash(t, note->client_id) % new_capacity;

            note->next = new_notes[new_index];
            new_notes[new_index] = note;

            note = next;
        } 
    }

    free(t->notes);
    t->notes = new_notes;
    t->cap = new_capacity;
}

int add_notes(HashTable *t, long client_id) {

    if (find_note(t, client_id) != NULL) {
        return 0;
    }

    if (t->size >= t->cap) {
        resize_table(t);
    }

    int index = get_hash(t, client_id);
    Note *new_note = (Note *)malloc(sizeof(Note));
    if (!new_note)
        return -1;

    new_note->client_id = client_id;
    new_note->state = (UserState *)malloc(sizeof(UserState));
    if (new_note->state == NULL) {
        return -1;
    } 
    new_note->state->boat_has = 0;
    new_note->state->peasant_side = 0;
    new_note->state->wolf_side = 0;
    new_note->state->goat_side = 0;
    new_note->state->cabbage_side = 0;
    new_note->next = t->notes[index];
    t->notes[index] = new_note;
    t->size++;

    return 0;
}

int take_wolf(msg_buffer *msg, UserState *state) {
    if (state->boat_has == 1) {
        strcpy(msg->msg, "Волк уже в лодке\n");
        return -1;
    } else if (state->boat_has) {
        strcpy(msg->msg, "Лодка занята!\n");
        return -1;
    }

    if (state->wolf_side == state->peasant_side) {
        state->boat_has = 1;
        state->wolf_side = 1;
        strcpy(msg->msg, "Волк в лодке\n");
        return 1;
    } else {
        strcpy(msg->msg, "Волк и лодка на разных сторонах!\n");
        return -1;
    }
}

int take_goat(msg_buffer *msg, UserState *state) {
    if (state->boat_has == 2) {
        strcpy(msg->msg, "Коза уже в лодке\n");
        return -1;
    } else if (state->boat_has) {
        strcpy(msg->msg, "Лодка занята!\n");
        return -1;
    }

    if (state->goat_side == state->peasant_side) {
        strcpy(msg->msg, "Коза в лодке\n");
        state->boat_has = 2;
        state->goat_side = 1;
        return 1;
    } else {
        strcpy(msg->msg, "Коза и лодка на разных сторонах!\n");
        return -1;
    }
}

int take_cabbage(msg_buffer *msg, UserState *state) {
    if (state->boat_has == 3) {
        strcpy(msg->msg, "Капуста уже в лодке\n");
        return -1;
    } else if (state->boat_has) {
        strcpy(msg->msg, "Лодка занята!\n");
        return -1;
    }

    if (state->cabbage_side == state->peasant_side) {
        state->boat_has = 3;
        state->cabbage_side = 1;
        strcpy(msg->msg, "Капуста в лодке\n");
        return 1;
    } else {
        strcpy(msg->msg, "Капуста и лодка на разных сторонах!\n");
        return -1;
    }
}

int take(msg_buffer *msg, HashTable *t) {

    UserState *client_state = get_user_state(msg, t);
    if (!client_state) {
        return -1;
    }

    if (!strcmp(msg->object, "wolf")) {
        return take_wolf(msg, client_state);
    } else if (!strcmp(msg->object, "goat")) {
        return take_goat(msg, client_state);
    } else if (!strcmp(msg->object, "cabbage")) {
        return take_cabbage(msg, client_state);
    } else {
        strcpy(msg->msg, "Нет такого объекта!\n");
        return -1;
    }
}

int put_wolf(msg_buffer *msg, UserState *state) {
    state->boat_has = 0;
    state->wolf_side = state->peasant_side;

    if (state->wolf_side == 2) {
        strcpy(msg->msg, "Волк на правом берегу\n");
        return 1;
    } else {
        strcpy(msg->msg, "Волк на левом берегу\n");
        return 1;
    }
}

int put_goat(msg_buffer *msg, UserState *state) {
    state->boat_has = 0;
    state->goat_side = state->peasant_side;

    if (state->goat_side == 2) {
        strcpy(msg->msg, "Коза на правом берегу\n");
        return 1;
    } else {
        strcpy(msg->msg, "Коза на левом берегу\n");
        return 1;
    }
}

int put_cabbage(msg_buffer *msg, UserState *state) {
    state->boat_has = 0;
    state->cabbage_side = state->peasant_side;

    if (state->cabbage_side == 2) {
        strcpy(msg->msg, "Капуста на правом берегу\n");
        return 1;
    } else {
        strcpy(msg->msg, "Капуста на левом берегу\n");
        return 1;
    }
}

int put(msg_buffer *msg, HashTable *t) {
    UserState *client_state = get_user_state(msg, t);
    if (!client_state) {
        return -1;
    } if (client_state->boat_has == 1) {
        put_wolf(msg, client_state);
    } else if (client_state->boat_has == 2) {
        put_goat(msg, client_state);
    } else if (client_state->boat_has == 3) {
        put_cabbage(msg, client_state);
    } else {
        strcpy(msg->msg, "Лодка пустая!\n");
        return -1;
    }
    if (client_state->wolf_side && client_state->cabbage_side && client_state->goat_side && client_state->peasant_side) {
        strcpy(msg->msg, "Победа! Никто никого не съел, все удачно перебрались на другой берег!\n");
        reset_user_state(client_state);
        return 1;
    }
    return 0;
}

int move(msg_buffer *msg, HashTable *t) {
    UserState *client_state = get_user_state(msg, t);
    if (!client_state) {
        return -1;
    }
    if (client_state->peasant_side == 0) {
        client_state->peasant_side = 2;
        strcpy(msg->msg, "Лодка на правом берегу.\n");
    } else {
        client_state->peasant_side = 0;
        strcpy(msg->msg, "Лодка на левом берегу.\n");
    }

    if (client_state->cabbage_side == client_state->goat_side && 
            client_state->goat_side != client_state->peasant_side) {
        strcpy(msg->msg, "Коза съела капусту. Попробуйте заново!\n");
        reset_user_state(client_state);
        return -1;
    } else if (client_state->wolf_side == client_state->goat_side && 
            client_state->goat_side != client_state->peasant_side) {
        strcpy(msg->msg, "Волк съел козу. Попробуйте заново!\n");
        reset_user_state(client_state);
        return -1;
    }

    return 0;
}

int game_process(msg_buffer *msg, HashTable *t) {
    if (add_notes(t, msg->client_id) == -1)
        return -1;

    if (!strcmp(msg->command, "move")) {
        return move(msg, t);
    } else if (!strcmp(msg->command, "put")) {
        return put(msg, t);
    } else if (!strcmp(msg->command, "take")) {
        return take(msg, t);
    } else {
        strcpy(msg->msg, "Такой команды нет!\n");
        reset_user_state(get_user_state(msg, t));
        return -1;
    }
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

int main() {

    int kill = 0;
    int exit = 0;
    pthread_t handler;

    int *args[2];
    args[0] = &kill;
    args[1] = &exit;

    pthread_create(&handler, NULL, handle, (void *)args);

    while (!exit) {
        key_t key = ftok("/tmp", PROJECT_ID);
        if (key == -1) {
            printf("Ошибка получения ключа\n");
            return -1;
        }

        int msgid = msgget(key, IPC_CREAT | 0666);
        if (msgid == -1) {
            printf("Ошибка в получении id очереди\n");
            return -1;
        }

        HashTable *table = create_table();
        if (!table) {
            msgctl(msgid, IPC_RMID, NULL);
            return -1;
        } printf("Сервер запущен. Ожидание PING...\n");
        msg_buffer message;

        kill = 0;

        while (!kill && !exit) {
            if (msgrcv(msgid, &message, sizeof(msg_buffer) - sizeof(long), 0, IPC_NOWAIT) == -1) {
                continue;
            }
            if (!strcmp(message.msg, "PING")) {
                int client_msgid = msgget(message.client_queue_key, 0666);
                if (client_msgid == -1) {
                    continue;
                }         
                    msg_buffer pong_msg = {
                    .mtype = 2,
                    .client_id = getpid(),
                    .status_code = 1,
                };
                strcpy(pong_msg.msg, "PONG");
                if (msgsnd(client_msgid, &pong_msg, sizeof(pong_msg) - sizeof(long), 0) == -1) {
                    continue;
                } else {
                    printf("Отправлен PONG в очередь %d\n", message.client_queue_key);
                } 
                continue;
            }     
            printf("Запрос от: %ld. Команда: %s\n", message.client_id, message.command);
            message.mtype = message.client_id;
            int res = game_process(&message, table);
            message.status_code = res;
            msgsnd(msgid, &message, sizeof(msg_buffer) - sizeof(long), 0);
        }

        free_table(table);
        msgctl(msgid, IPC_RMID, NULL);
        printf("Сервер завершил работу.\n");
    }

    pthread_join(handler, NULL);
    return 0; 
}