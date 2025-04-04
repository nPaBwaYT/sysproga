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
#include <sys/wait.h>


void *listen(void *args) {

    int *exit = ((int **)args)[0];
    int *server_in = ((int **)args)[1];

    char cmd;

    while (!(*exit)) {
        cmd = fgetc(stdin);

        if (cmd == 'e' || cmd == -1) {
            write(*server_in, "kill\n", 1);
            *exit = 1;
        } else {
            write(*server_in, &cmd, 1);
        }
    }
    return NULL;
}



int main(int argc, char *argv[]) {
    if (argc != 2) {
        return -1;
    }

    int exit = 0;

    int to_server[2];
    int from_server[2];

    pipe(to_server);
    pipe(from_server);

    
    dup2(STDOUT_FILENO, from_server[0]);

    int *args[2] = {&exit, to_server + 1};

    pthread_t listener;
    pthread_create(&listener, NULL, listen, (void *)(args));

    while (!exit) {
        pid_t pid = fork();

        switch (pid)
        {
        case -1:
            printf("fork error\n");
            sleep(2);
            break;

        case 0:
            pthread_cancel(listener);

            dup2(to_server[0], STDIN_FILENO);
            dup2(STDOUT_FILENO, from_server[1]);

            close(to_server[1]);
            close(from_server[0]);

            char *new_argv[2] = {argv[1], NULL};

            if (execve(argv[1], new_argv, NULL) == -1) {
                printf("execve error\n");
                return -1;
            }
            break;

        default:
            waitpid(pid, NULL, 0);
            break;
        }
    }
    pthread_join(listener, NULL);

    close(to_server[1]);
    close(from_server[0]);
    close(to_server[0]);
    close(from_server[1]);

    return 0;
}