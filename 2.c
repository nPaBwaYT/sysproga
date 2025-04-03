#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>


int validate_hex(char *hex) {
    if (strlen(hex) != 8) {
        return 0;
    }

    for (int i = 0; i < 8; i++) {
        if (!((hex[i] >= '0' && hex[i] <= '9') || 
              (hex[i] >= 'a' && hex[i] <= 'f') || 
              (hex[i] >= 'A' && hex[i] <= 'F'))) {
            return 0;
        }
    }

    return 1;
}


void files_close(int * files, int files_count) {
    for (int j = 0; j < files_count; ++j) {
        close(files[j]);
    }
}


int files_open(int * files, int files_count, char * paths[]) {

    for (int i = 0; i < files_count; ++i) {

        if ((files[i] = open(paths[i], O_RDONLY)) < 0) {
            fprintf(stderr, "file error\n");

            files_close(files, i);

            return 1;
        }
    }

    return 0;
}


int make_copy(int file, char *path, int *files, int files_count, int copy_n) {
    char buff[256];
    for (int i = 0; i < 256; ++i) {
        buff[i] = '\0';
    }

    int out;
    char number[2];
    number[1] = '\0';
    sprintf(number, "%i", copy_n);

    char newpath[256];
    strcpy(newpath, path);
    strcat(newpath, number);

    if ((out = open(newpath, O_WRONLY|O_CREAT, S_IRWXU)) < 0) {
        fprintf(stderr, "file error\n");
        files_close(files, files_count);
        return 1;
    }
    
    int bytes_read;
    while ((bytes_read = read(file, buff, 256)) > 0) {
        write(out, buff, bytes_read);
    }
    close(out);

    return 0;
}



int find_string(char *paths[], int files_count, char *some_string, int *pipefds) {
    pid_t pid;

    for (int i = 0; i < files_count; ++i) {

        pid = fork();
        
        switch (pid)
        {
        case -1:
            fprintf(stderr, "fork error\n");
            return 1;

        case 0:
            close(pipefds[0]);
            dup2(STDOUT_FILENO, pipefds[1]);
            char * new_argv[4];
            new_argv[0] = "find.out";
            new_argv[1] = some_string;
            new_argv[2] = paths[i];
            new_argv[3] = NULL;

            if (execve("./output/find.out", new_argv, NULL)) {
                fprintf(stderr, "execve error\n");
            }
            return 1;
        
        default:
            close(pipefds[1]);
            dup2(STDOUT_FILENO, pipefds[0]);
            break;
        }
    }

    for (int i = 0; i <= files_count; ++i) {
        wait(NULL);
    }
    return 0;
}


pid_t copyN(char * paths[], int files_count, int n) {
    pid_t *pids = (pid_t *)calloc(n + 1, sizeof(pid_t));
    if (pids == NULL) {
        fprintf(stderr, "calloc error\n");
        return 1;
    }

    int *files = (int *)calloc(files_count, sizeof(int));
    if (files == NULL) {
        fprintf(stderr, "calloc error\n");
        return 1;
    };

    for (int copy_n = 1; copy_n <= n; ++copy_n) {
        pids[copy_n] = fork();

        switch (pids[copy_n])
        {
        case -1:
            fprintf(stderr, "fork error\n");
            free(pids);
            free(files);
            return 1;
        
        case 0:
            
            if (files_open(files, files_count, paths)) {
                free(pids);
                free(files);
                return 1;
            }

            for (int k = 0; k < files_count; ++k) {
                if (make_copy(files[k], paths[k], files, files_count, copy_n)) {
                    free(pids);
                    free(files);
                    return 1;
                }
            }

            files_close(files, files_count);
            free(pids);
            free(files);
            return 1;

        default:
            break;
        }
    }

    for (int copy_n = 1; copy_n <= n; ++copy_n) {
        waitpid(pids[copy_n], NULL, 0);
    }
    free(pids);
    free(files);
    return 0;
}

long long xorN(int n, int files_count, int * files) {
    
    unsigned int blocksize_bit = (1 << n);
    unsigned int blocksize_byte = (blocksize_bit - 1) / 8 + 1;
    int fd;
    unsigned long long block = 0;
    unsigned long long sum = 0;
    unsigned long long mask;

    mask = (~((~0ull) << (blocksize_bit - 1))) * 2 + 1;
    
    for (int file = 0; file < files_count; ++file) {

        fd = files[file];

        while (read(fd, &block, blocksize_byte) > 0) {
            for (unsigned int k = 0; k < (blocksize_byte * 8) / blocksize_bit; ++k) {
                   sum = sum ^ (mask & (block >> k * blocksize_bit));
            }

            block = 0;
        } 
    }

    return sum;
}


int count_mask(int *files, int files_count, int mask) {
    int buff;
    int count = 0;

    for (int k = 0; k < files_count; ++k) {
        while ((read(files[k], &buff, 4)) != 0) {
            if ((buff & mask) == buff) {
                ++count;
            }
        }
    }
    return count;
}


int main(int argc, char * argv[]) {

    if (argc < 3) {
        fprintf(stderr, "incorrect usage\n");
        return 1;
    }

    if ((strncmp(argv[argc - 1], "xor", 3)) == 0) {

        int files_count = argc - 2;
        int n = atoi(&argv[argc - 1][3]);
        if (n < 1 || n > 6) {
            fprintf(stderr, "invalid arg\n");
            return 1;
        }

        int *files = (int *)calloc(files_count, sizeof(int));
        if (files == NULL) {
            fprintf(stderr, "calloc error\n");
            return 1;
        };

        if (files_open(files, files_count, &argv[1])) {
            free(files);
            return 1;
        }
        
        long long sum = xorN(n, files_count, files);

        files_close(files, files_count);
        free(files);

        char repr[256];
        for (int i = 0; i < 256; ++i) {
            repr[i] = '\0';
        } 

        sprintf(repr, "%lli\n", sum);

        write(STDOUT_FILENO, repr, sizeof(repr) - 1);

    } else if ((strncmp(argv[argc - 1], "copy", 4)) == 0) {

        int files_count = argc - 2;

        int n = atoi(&argv[argc - 1][4]);
        if (n < 1 || n > 20) {
            fprintf(stderr, "invalid arg\n");
            return 1;
        }

        if (copyN(&argv[1], files_count, n) != 0) {
            return 0; // убийство детей
        }

    } else if ((strncmp(argv[argc - 2], "find", 4)) == 0) {

        int files_count = argc - 3;

        char * some_string = argv[argc - 1];
        int len = strlen(some_string) + 1;
        for (char * ptr = some_string; *ptr != '\0'; ++ptr) {
            if (*ptr == '\\') {
                switch (ptr[1])
                {
                case 'n':
                    memmove(ptr, &ptr[1], len - (ptr - some_string) - 1);
                    *ptr = '\n';
                    break;
                case 't':
                    memmove(ptr, &ptr[1], len - (ptr - some_string) - 1);
                    *ptr = '\t';
                    break;
                case '0':
                    memmove(ptr, &ptr[1], len - (ptr - some_string) - 1);
                    *ptr = '\0';
                    break;
                default:
                    break;
                }
            }
        }

        printf("searching :\n[%s]\n", some_string);

        int pipefds[2];
        pipe(pipefds);

        if (find_string(&argv[1], files_count, some_string, pipefds) != 0) {
            return 0; // убийство детей
        }

        close(pipefds[0]);

    }  else if ((strncmp(argv[argc - 2], "mask", 4)) == 0) {
        int files_count = argc - 3;

        if (!validate_hex(argv[argc-1])) {
            fprintf(stderr, "invalid arg\n");
            return 1;
        }
        int mask = (int)strtol(argv[argc-1], NULL, 16);

        int *files = (int *)calloc(files_count, sizeof(int));
        if (files == NULL) {
            fprintf(stderr, "calloc error\n");
            return 1;
        };

        if (files_open(files, files_count, &argv[1])) {
            free(files);
            return 1;
        }
                                                                        
        char answer[256];
        for (int i = 0; i < 256; ++i) {
            answer[i] = 0;  
        }
        sprintf(answer, "%i", count_mask(files, files_count, mask));

        write(STDOUT_FILENO, answer, sizeof(answer) - 1);
        putchar('\n');

        files_close(files, files_count);
        free(files);
    
    } else {
        fprintf(stderr, "invalid arg\n");
        return 1;
    }

    return 0;
}