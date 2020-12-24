#include "cached_zip.h"

#include "apr_pools.h"
#include "apr_hash.h"
#include "apr_thread_mutex.h"
#include "apr_strings.h"

#include <stdio.h>

static void mylog(const char *msg) {
    printf(msg);
    printf("\n");
    fflush(stdout);
}

static apr_pool_t *pool;
static apr_hash_t *hash;
static apr_thread_mutex_t *mutex;

zip_t * cached_zip_open(const char *path, int flags, int *errorp) {
    mylog("cached_zip_open");
    apr_thread_mutex_lock(mutex);
    zip_t *result;
    result = apr_hash_get(hash, path, APR_HASH_KEY_STRING);
    if(result) {
        mylog("Using opened zip");
        apr_thread_mutex_unlock(mutex);
        return result;
    } else {
        mylog("Opening zip");
        char *key = apr_pstrdup(pool, path);
        result = zip_open(key, flags, errorp);
        if(result) {
            apr_hash_set(hash, key, APR_HASH_KEY_STRING, result);
        }
        apr_thread_mutex_unlock(mutex);
        return result;
    }
}

void initialize_zip_cache(apr_pool_t *argpool) {
    mylog("Initializing cache");
    pool = argpool;
    hash = apr_hash_make(pool);
    apr_thread_mutex_create(&mutex, APR_THREAD_MUTEX_UNNESTED, pool);
    mylog("Cache initialized");
}