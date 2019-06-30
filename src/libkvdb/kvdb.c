#include "kvdb.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <unistd.h>

enum DBState
{
    Opened = 1,
    Closed = 0,
};

char *find_db(kvdb_t *db, const char *target_key)
{
    static char buf[512], key[512], end[16];
    char *res = NULL;
    FILE *file = fopen(db->filename, "r");

    if (file == NULL)
    {
        return NULL;
    }

    flock(fileno(file), LOCK_EX);

    while (!feof(file))
    {
        key[0] = '\0';
        int vlen = 0;
        if (fgets(buf, 500, file) == NULL)
            continue;
        int cread = sscanf(buf, "%d:%[^\n]\n", &vlen, key);
        if (cread != 2)
            continue;
        if (strcmp(target_key, key) != 0)
        {
            continue;
        }
        char *value = malloc(vlen + 5);
        if (fgets(value, vlen + 5, file) == NULL)
        {
            free(value);
            continue;
        }
        value[vlen] = '\0';
        if (fgets(end, 10, file) == NULL)
        {
            free(value);
            continue;
        }
        end[3] = '\0';
        if (strcmp(end, "END") != 0)
        {
            free(value);
            continue;
        }
        free(res);
        res = value;
    }

    flock(fileno(file), LOCK_UN);

    fclose(file);
    return res;
}

int kvdb_open(kvdb_t *db, const char *filename)
{
    if (db == NULL)
        return -1;

    if (db->state == Opened)
    {
        return -1;
    }

    pthread_mutex_init(&db->mutex, NULL);

    pthread_mutex_lock(&db->mutex);

    if (strlen(filename) >= 1024)
    {
        return -1;
    }

    strcpy(db->filename, filename);

    FILE *file = fopen(db->filename, "a+");

    if (file == NULL)
    {
        return -1;
    }

    fclose(file);

    db->state = Opened;

    pthread_mutex_unlock(&db->mutex);

    return 0;
}

int kvdb_close(kvdb_t *db)
{
    if (db == NULL || db->state != Opened)
        return 0;
    pthread_mutex_lock(&db->mutex);

    db->filename[0] = '\0';

    db->state = Closed;

    pthread_mutex_unlock(&db->mutex);

    return 0;
}

int kvdb_put(kvdb_t *db, const char *key, const char *value)
{
    if (db == NULL || key == NULL || value == NULL)
        return -1;
    {
        int lkey = strlen(key);
        if (lkey == 0 || lkey > 128 || strlen(value) > 16 * 1024 * 1024)
            return -1;
    }

    pthread_mutex_lock(&db->mutex);

    FILE *file = fopen(db->filename, "a");

    if (file == NULL)
    {
        return -1;
    }

    flock(fileno(file), LOCK_EX);

    fputs("\n", file);

    fprintf(file, "%d:%s\n", (int)strlen(value), key);
    fprintf(file, "%s\n", value);
    sync();
    fputs("END\n", file);
    sync();

    flock(fileno(file), LOCK_UN);

    fclose(file);

    pthread_mutex_unlock(&db->mutex);

    return 0;
}

char *kvdb_get(kvdb_t *db, const char *key)
{
    if (db == NULL || key == NULL)
        return NULL;
    {
        int lkey = strlen(key);
        if (lkey == 0 || lkey > 128)
            return NULL;
    }

    pthread_mutex_lock(&db->mutex);

    char *value = find_db(db, key);

    pthread_mutex_unlock(&db->mutex);

    return value;
}
