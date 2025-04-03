#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>


void computeLPSArray(const char *pattern, int m, int *lps) {
    int len = 0; 
    lps[0] = 0;  
    int i = 1;

    
    while (i < m) {
        if (pattern[i] == pattern[len]) {
            len++;
            lps[i] = len;
            i++;
        } else {
            if (len != 0) {
                len = lps[len - 1];  
            } else {
                lps[i] = 0;
                i++;
            }
        }
    }
}

int KMPSearch(const char *pattern, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Ошибка при открытии файла\n");
        return -1;
    }

    
    int m = strlen(pattern);
    int *lps = (int *)malloc(m * sizeof(int));  
    if (lps == NULL) {
        perror("Ошибка выделения памяти для lps\n");
        fclose(file);
        return -1;
    }

    
    computeLPSArray(pattern, m, lps);

    int found = 0;
    int j = 0;  

    char ch;
    while ((ch = fgetc(file)) != EOF) {
        
        while (j > 0 && pattern[j] != ch) {
            j = lps[j - 1];  
        }

        if (pattern[j] == ch) {
            j++;
        }

        
        if (j == m) {
            found = 1;
            break;
        }
    }

    free(lps);
    fclose(file); 
    return found;
}


int main(int argc, char * argv[]) {
    if (argc != 3) {
        printf("Ошибка использования\n");
        return -1;
    }
    if (KMPSearch(argv[1], argv[2]) == 1) {
        printf("%s\n", argv[2]);
    }
}
