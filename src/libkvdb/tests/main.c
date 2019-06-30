#include "kvdb.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>

char *randStr(int maxLen)
{
    int n = rand() % maxLen + 1;
    char *res = malloc(n + 1);
    for (int i = 0; i < n; i++)
        res[i] = rand() % 26 + 'a';
    res[n] = '\0';
    return res;
}

kvdb_t db;

void basic_test()
{
    const char *key = "operating-systems";
    const char *v1 = "three-easy-pieces";
    const char *v2 = "three-easy-pieces updated";
    char *value;

    assert(kvdb_open(&db, "a.db") == 0);

    assert(kvdb_get(&db, key) == NULL);

    assert(kvdb_put(&db, key, v1) == 0);
    value = kvdb_get(&db, key);
    assert(strcmp(value, v1) == 0);
    free(value);

    assert(kvdb_put(&db, key, v2) == 0);
    value = kvdb_get(&db, key);
    assert(strcmp(value, v2) == 0);
    free(value);

    assert(kvdb_close(&db) == 0);
}

void *thread_work(void *_id)
{
    int id = *((int *)_id);
    int rc = rand() % 10 + 1;
    // printf("thread-%d want to add %d items.\n", id, rc);
    for (int i = 0; i < rc; i++)
    {
        char *key = randStr(128);
        char *value = randStr(1024*1024);

        int ans = kvdb_put(&db, key, value);
        // printf("thread-%d put and gets %d.\n", id, ans);
        assert(ans == 0);

        char *tvalue = kvdb_get(&db, key);
        ans = strcmp(value, tvalue);
        // printf("thread-%d get and comparer gets %d.\n", id, ans);
        assert(ans == 0);

        free(tvalue);
        free(key);
        free(value);
    }
    return NULL;
}

void multi_thread()
{
#define PN 8
    static int id[PN];
    pthread_t ps[PN];

    assert(kvdb_open(&db, "b.db") == 0);

    for (int i = 0; i < PN; i++)
    {
        id[i] = i;
        assert(pthread_create(ps + i, NULL, thread_work, id + i) == 0);
    }
    for (int i = 0; i < PN; i++)
    {
        pthread_join(ps[i], NULL);
    }

    assert(kvdb_close(&db) == 0);
}

int main()
{
    srand(2134567);
    basic_test();
    puts("Basic test PASS.");

    multi_thread();
    puts("Multi-thread test PASS.");
    return 0;
}