#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>


#define LOGIN_SIZ 7
#define PIN_SIZ 6
#define TABLE_SIZ 128
#define BUFF_SIZ 64
#define HASH_CONST 23719 * 23719
#define SALT_SIZE 16


int bytes_to_hex_repr(const char *bytes, long int max_len_bytes, char *destination) {

    for (const char *ptr = bytes; 
        (ptr - bytes) < (long int)strlen(bytes) && (ptr - bytes) < max_len_bytes; 
         ptr += sizeof(unsigned int))
    {
        snprintf(destination + 2 * (ptr - bytes), 9, "%08x", *(unsigned int*)ptr);
    }
    return 0;
}

int hex_repr_to_bytes(const char *repr, long int max_len_dest, char *destination) {

    for (char *ptr = destination; 
        (ptr - destination) < (long int)(strlen(repr) / 2) && (ptr - destination) < max_len_dest;
         ptr += sizeof(unsigned int))
    {
        sscanf(repr + 2 * (ptr - destination), "%08x", (unsigned int *)ptr);
    }
    return 0;
}

typedef struct Note
{
    char *PIN_hash;
    int limit;
    char *username;
    int is_occupied;
    struct Note *next;
} Note;

typedef struct
{
    Note **notes;
    int size;
    int cap;

} HashTable;

HashTable *create_table()
{

    HashTable *table = (HashTable *)malloc(sizeof(HashTable));
    if (table == NULL)
        return NULL;
    table->notes = (Note **)calloc(TABLE_SIZ, sizeof(Note *));
    if (table->notes == NULL)
    {
        free(table);
        return NULL;
    }
    table->cap = TABLE_SIZ;
    table->size = 0;
    return table;
}

void free_table(HashTable *t)
{
    if (!t || !t->notes)
        return;

    for (int i = 0; i < t->cap; i++)
    {
        Note *current = t->notes[i];
        while (current)
        {
            Note *temp = current;
            current = current->next;
            free(temp->username);
            free(temp->PIN_hash);
            free(temp);
        }
    }
    free(t->notes);
    free(t);
}

int get_hash(HashTable *t, char *login)
{

    int index = 0;
    char *ptr = login;
    while (*ptr != '\0')
    {
        index += *ptr;
        ptr++;
    }
    return index % t->cap;
}

unsigned int simple_hash(const char *input, const char *salt)
{
    unsigned int hash = HASH_CONST;
    while (*input)
    {
        hash = (hash << 5) + hash + (unsigned char)(*input);
        input++;
    }
    
    char * ptr = (char *)&hash;

    for (size_t i = 0; i < strlen(salt); ++i) {
        ptr[i % 4] = ptr[i % 4] ^ salt[i];
    } 
    return hash;
}


int generate_salt(char *salt, size_t salt_size)
{
    for (size_t i = 0; i < salt_size; i++)
    {
        salt[i] = (char)(rand() % 256);
    }
    salt[salt_size] = '\0';
    return 0;
}

int encrypt_password(const char *password, char **encrypted_password)
{
    
    char salt[SALT_SIZE + 1]; 
    if (generate_salt(salt, SALT_SIZE) != 0)
    {
        return -1; 
    }

    unsigned int hash_value = simple_hash(password, salt);

    *encrypted_password = (char *)calloc(2 * sizeof(unsigned int) + 2 * SALT_SIZE + 1, sizeof(char)); 
    if (*encrypted_password == NULL)
    {
        return -1;
    }

    bytes_to_hex_repr(salt, SALT_SIZE, *encrypted_password);
    snprintf(*encrypted_password + 2 * strlen(salt), 9, "%08x", hash_value);

    return 0;
}

int compare_passwords(const char *password, const char *hashed_password, int *compare_res)
{

    char salt[SALT_SIZE + 1];
    hex_repr_to_bytes(hashed_password, SALT_SIZE, salt);
    salt[SALT_SIZE] = '\0';

    unsigned int entered_hash = simple_hash(password, salt);

    char entered_hash_hex[9]; 
    snprintf(entered_hash_hex, 9, "%08x", entered_hash);
    
    *compare_res = strcmp(hashed_password + 2 * strlen(salt), entered_hash_hex);

    return 0;
}

int read_line(char * dest, int max_len) {
    char *line = NULL;
    size_t bytes_allocated;
    int bytes_read;

    bytes_read = getline(&line, &bytes_allocated, stdin);
    strncpy(dest, line, max_len - 1);
    free(line);

    dest[max_len - 1] = '\0';
    char *ptr = dest;
    do {
        if (*ptr == '\n') {
            *ptr = '\0';
            break;
        }
    } while (*(ptr++) != '\0');

    return bytes_read;
}


Note *find_note(HashTable *t, char *username)
{
    int index = get_hash(t, username);
    Note *note = t->notes[index];

    while (note != NULL)
    {
        if (strcmp(note->username, username) == 0)
        {
            return note;
        }
        note = note->next;
    }

    return NULL;
}

void resize_table(HashTable *t)
{
    int old_capacity = t->cap;
    int new_capacity = old_capacity * 2;

    Note **new_notes = (Note **)calloc(new_capacity, sizeof(Note *));
    if (!new_notes)
    {
        printf("Ошибка выделения памяти при увеличении таблицы\n");
        return;
    }

    for (int i = 0; i < old_capacity; i++)
    {
        Note *note = t->notes[i];
        while (note)
        {
            Note *next = note->next;
            int new_index = get_hash(t, note->username) % new_capacity;

            note->next = new_notes[new_index];
            new_notes[new_index] = note;

            note = next;
        }
    }

    free(t->notes);
    t->notes = new_notes;
    t->cap = new_capacity;
}

int add_notes(HashTable *t, char *login, char *PIN, int limit)
{

    if (t->size >= t->cap)
    {
        resize_table(t);
    }

    int index = get_hash(t, login);
    Note *new_note = (Note *)malloc(sizeof(Note));
    if (!new_note)
        return -1;

    new_note->username = strdup(login);
    if (!new_note->username)
    {
        free(new_note);
        return -1;
    }

    new_note->PIN_hash = strdup(PIN);
    if (!new_note->PIN_hash)
    {
        free(new_note->username);
        free(new_note);
        return -1;
    }

    new_note->limit = limit;
    new_note->is_occupied = 1;
    new_note->next = t->notes[index];
    t->notes[index] = new_note;
    t->size++;

    return 0;
}

int read_in_table(FILE *f, HashTable *t)
{
    char username[LOGIN_SIZ], PIN[256];
    int limit;
    int count = 0;

    while (fscanf(f, "%s %s %d", username, PIN, &limit) == 3)
    {

        add_notes(t, username, PIN, limit);
        count++;
    }

    return count;
}

int write_in_file(FILE *f, HashTable *t)
{
    for (int i = 0; i < t->cap; i++)
    {
        Note *note = t->notes[i];
        while (note != NULL)
        {
            if (note->is_occupied == 1)
            {
                fprintf(f, "%s %s %d\n", note->username, note->PIN_hash, note->limit);
            }
            note = note->next;
        }
    }
    return 1;
}

int check_in_table(HashTable *t, char *login, char *PIN)
{
    int compare_res = 0;
    int index = get_hash(t, login);
    Note *note = t->notes[index];

    if (note == NULL)
    {
        return -2;
    }

    while (note != NULL && strcmp(note->username, login) != 0)
    {
        note = note->next;
    }

    if (note == NULL)
    {
        return -2;
    }

    if (note->is_occupied)
    {
        if (compare_passwords(PIN, note->PIN_hash, &compare_res))
        {
            return -1;
        }

        return (compare_res == 0) ? 1 : -1;
    }

    return -2;
}

void print_table(HashTable *t)
{

    for (int i = 0; i < t->cap; i++)
    {
        Note *note = t->notes[i];
        while (note != NULL)
        {
            if (note->is_occupied == 1)
            {
                printf("%s: %s (%d)\n", note->username, note->PIN_hash, note->limit);
            }
            note = note->next;
        }
    }
}

int is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int validate_date(const char *date)
{
    if (date == NULL || strlen(date) != 10 || date[2] != ':' || date[5] != ':')
    {
        return 0;
    }

    int day, month, year;
    sscanf(date, "%d:%d:%d", &day, &month, &year);

    if (year < 1 || year > 9999)
    {
        return 0;
    }

    if (month < 1 || month > 12)
    {
        return 0;
    }

    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (is_leap_year(year))
    {
        days_in_month[1] = 29;
    }

    if (day < 1 || day > days_in_month[month - 1])
    {
        return 0;
    }

    return 1;
}

int validate_login(char *login)
{
    if (strlen(login) > 6)
    {
        return 0;
    }
    if (!login || !*login)
    {
        return 0;
    }

    for (int i = 0; login[i] != '\0'; i++)
    {
        if (!isalnum((unsigned char)login[i]))
        {
            return 0;
        }
    }

    return 1;
}

int validate_PIN(char *str, int PIN)
{
    if (PIN < 0 || PIN > 99999)
    {
        return 0;
    }

    for (size_t i = 0; i < strlen(str); i++)
    {
        if (!isdigit((unsigned char)str[i]))
        {
            return 0;
        }
    }
    return 1;
}

int menu_auth(char *login, char *PIN)
{
    char *endptr;
    int bytes_read;
    printf("Введите логин (не более 6 символов): ");
    
    if ((bytes_read = read_line(login, LOGIN_SIZ)) > LOGIN_SIZ) {
        printf("Ошибка: логин слишком большой\n");
        return 0;
    } else if (bytes_read == -1) {
        printf("\nEOF\n");
        return -1;
    }

    if (!validate_login(login))
    {
        printf("Ошибка: неверный формат логина!\n");
        return 0;
    }

    printf("Введите PIN (0-100000): ");
    if ((bytes_read = read_line(PIN, PIN_SIZ)) > PIN_SIZ) {
        printf("Ошибка: пин слишком большой\n");
        return 0;
    } else if (bytes_read == -1) {
        printf("\nEOF\n");
        return -1;
    }

    int num = strtol(PIN, &endptr, 10);
    if (*endptr != '\0')
    {
        printf("Ошибка: строка содержит недопустимые символы после числа\n");
        return 0;
    }

    if (!validate_PIN(PIN, num))
    {
        printf("Неверный пин-код\n");
        return 0;
    }
    return 1;
}

void print_current_datetime(char *type)
{
    time_t t;
    struct tm *tm_info;

    t = time(NULL);
    tm_info = localtime(&t);
    if (!strcmp(type, "Time"))
    {
        printf("Текущее время: %02d:%02d:%02d\n",
               tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    }
    else if (!strcmp(type, "Date"))
    {
        printf("Текущая дата: %02d:%02d:%02d\n",
               tm_info->tm_mday, tm_info->tm_mon + 1, tm_info->tm_year % 100);
    }
}

void print_elapsed_time(const char *date_str, const char *flag)
{
    struct tm input_time = {0};
    time_t input_epoch, current_epoch;
    double seconds_diff;

    if (sscanf(date_str, "%d:%d:%d", &input_time.tm_mday, &input_time.tm_mon, &input_time.tm_year) != 3)
    {
        printf("Ошибка: Неверный формат даты. Используйте ДД:ММ:ГГГГ\n");
        return;
    }

    input_time.tm_mon -= 1;
    input_time.tm_year -= 1900;

    input_epoch = mktime(&input_time);
    if (input_epoch == -1)
    {
        printf("Ошибка: Неверная дата\n");
        return;
    }

    current_epoch = time(NULL);

    seconds_diff = difftime(current_epoch, input_epoch);

    if (strcmp(flag, "-s") == 0)
    {
        printf("Прошло %.0f секунд\n", seconds_diff);
    }
    else if (strcmp(flag, "-m") == 0)
    {
        printf("Прошло %.0f минут\n", seconds_diff / 60);
    }
    else if (strcmp(flag, "-h") == 0)
    {
        printf("Прошло %.0f часов\n", seconds_diff / 3600);
    }
    else if (strcmp(flag, "-y") == 0)
    {
        printf("Прошло %.2f лет\n", seconds_diff / (365.25 * 24 * 3600));
    }
    else
    {
        printf("Ошибка: Неизвестный флаг. Используйте -s, -m, -h или -y\n");
    }
}

int sanctions_user(HashTable *t, char *username, int number)
{

    Note *note = find_note(t, username);
    if (note->is_occupied != 1)
    {
        return -1;
    }
    note->limit = number;
    return 1;
}

int check_limit(int limit)
{
    return (limit == -1) ? limit : --limit;
}

int procces(HashTable *t, char *login)
{
    printf("\nВам доступны следущие команды:\n");
    printf("Time - запрос текущего времени в стандартном формате чч:мм:сс\n");
    printf("Date - запрос текущей даты в стандартном формате дд:мм:гг\n");
    printf("Howmuch <time> flag - запрос прошедшего времени с указанной даты в параметре <time>, параметр flag определяет тип представления результата (-s в секундах, -m в минутах, -h в часах, -y в годах)\n");
    printf("Logout - выйти в меню авторизации\n");
    printf("Sanctions username <number> - данный пользователь не может в одном сеансе выполнить более <number> запросов\n");
    printf("exit - выход из программы\n");

    char line[BUFF_SIZ];
    Note *note = find_note(t, login);
    int limit = note->limit;
    int bytes_read;
    if (limit != -1)
    {
        printf("Количество доступных команд: %d\n", limit);
    }

    while (1)
    {
        if (limit == -1 || limit > 0)
        {   
            if ((bytes_read = read_line(line, BUFF_SIZ)) > BUFSIZ) {
                printf("Команда слишком длинная\n");
                continue;

            } else if (bytes_read == -1) {
                printf("\nEOF\n");
                return -1;
            }

            if (*line == '\0') {
                continue;
            }
        }
        else
        {
            return -2;
        }


        if (!strcmp(line, "Time"))
        {
            print_current_datetime(line);
            limit = check_limit(limit);
        }
        else if (!strcmp(line, "Date"))
        {
            print_current_datetime(line);
            limit = check_limit(limit);
        }
        else if (!strcmp(line, "Logout"))
        {
            return 0;
        }
        else if (!strcmp(line, "exit"))
        {
            return 1;
        }
        else if (!strncmp(line, "Howmuch", strlen("Howmuch")))
        {
            char date[9];
            char flag[3];
            if (sscanf(line + strlen("Howmuch "), "%s %s %s", date, flag, line) != 2) {
                printf("Некорректное использование\n");
                continue;
            }
            if (!validate_date(date))
            {
                printf("Неккоректная дата\n");
                continue;
            }
            print_elapsed_time(date, flag);
            limit = check_limit(limit);
        }
        else if (!strncmp(line, "Sanctions", strlen("Sanctions")))
        {
            char user[LOGIN_SIZ];
            int number, key;
            if (sscanf(line + strlen("Sanctions "), "%s %d %s", user, &number, line) != 2) {
                printf("Некорректное использование\n");
                continue;
            }

            printf("Введите значение для подтверждения: ");

            if ((bytes_read = read_line(line, BUFF_SIZ)) > BUFSIZ) {
                printf("Ключ слишком длинный\n");
                continue;
                
            } else if (bytes_read == -1) {
                printf("\nEOF\n");
                return -1;
            }

            if (!validate_PIN(line, 1) || (key = atoi(line)) != 12345)
            {
                printf("Неверный ключ\n");
                continue;
            }
            int res = sanctions_user(t, user, number);
            if (res == -1)
            {
                printf("Пользователь не найден\n");
            }
            else
            {
                printf("Ограничения введены\n");
                limit = check_limit(limit);
            }
            
        }
        else
        {
            printf("Такой команды нет\n");
        }
    }
}

int main()
{
    FILE *rd = fopen("/dev/random", "r");
    unsigned int seed;
    for (char *ptr = (char *)&seed; ptr - (char *)&seed < 4; ++ptr) {
        *ptr = fgetc(rd);
    }
    fclose(rd);
    srand(seed);

    char login[LOGIN_SIZ] = {0};
    char pin[PIN_SIZ];
    int on_exit = 0;

    HashTable *table = create_table();
    FILE *f = fopen("users", "r");
    if (!f)
    {
        return -1;
    }

    read_in_table(f, table);
    fclose(f);
    print_table(table);
    while (!on_exit)
    {
        switch (menu_auth(login, pin))
        {
        case -1:
            on_exit = 1;
            continue;

        case 0:
            continue;
        
        case 1:
            break;
        }

        int res = check_in_table(table, login, pin);
        if (res == 1)
        {
            printf("Успешная авторизация\n");
            res = procces(table, login);

            switch (res)
            {
            case -1:
                on_exit = 1;
                continue;
            
            case 0:
                continue;
            
            case 1:
                on_exit = 1;
                continue;;
        
            case -2:
                printf("У вас закончились доступные команды\n");
                continue;
            }
        }
        else if (res == -1)
        {
            printf("Неверный пароль или логин\n");
            continue;
        }

        printf("Такой пользователь не найден. Хотите зарегистрироваться?[Y/N]\n");
        char c[2];
        int bytes_read;

        while (1) {
            if ((bytes_read = read_line(c, 2)) > 2) {
                printf("Русским языком спрошено: хотите зарегистрироваться?![Y/N]\n");
                continue;

            } else if (bytes_read == -1) {
                printf("\nEOF\n");
                on_exit = 1;
                break;
            }
        
            if (*c == 'Y' || *c == 'y')
            {
                char *PIN_hash = NULL;
                if (encrypt_password(pin, &PIN_hash))
                {
                    return -1;
                }
                add_notes(table, login, PIN_hash, -1);
                print_table(table);
                free(PIN_hash);
                break;
            }
            else if (*c == 'N' || *c == 'n')
            {
                printf("Всего доброго\n");
                on_exit = 1;
                break;
            }
            else
            {
                printf("Русским языком спрошено: хотите зарегистрироваться?![Y/N]\n");
                continue;
            }
        }
    }

    f = fopen("users", "w");
    if (!f)
    {
        return -1;
    }
    write_in_file(f, table);
    fclose(f);
    free_table(table);
    return 0;
}
