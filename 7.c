#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

void list_files(char *path)
{
    printf("%s:\n", path);
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

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Input names of directories\n");
        return -1;
    }
    for (int i = 1; i < argc; i++)
    {
        list_files(argv[i]);
        printf("\n");
    }

    return 0;
}
