#ifndef __KVDB_H__
#define __KVDB_H__

#include <pthread.h>
#include <stdio.h>

struct kvdb {
    pthread_mutex_t mutex;
    int state;
    // FILE* file;
    char filename[1024];
    // void* data;
};
typedef struct kvdb kvdb_t;

int kvdb_open(kvdb_t *db, const char *filename);
int kvdb_close(kvdb_t *db);
int kvdb_put(kvdb_t *db, const char *key, const char *value);
char *kvdb_get(kvdb_t *db, const char *key);

#endif
