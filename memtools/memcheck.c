#include "memcheck.h"
#include "hashtable_memcheck.h"
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>

static HashTable *mem_dict;
const size_t init_size = 100;
static int isinit;
static pthread_mutex_t big_lock; 

#define MEMCHECK_UTILITY        \
    memcheck_source_t *souc = (memcheck_source_t*) malloc(sizeof(memcheck_source_t)); \
    souc->filename = filename;  \
    souc->lineno = lineno;      \
    souc->funcname = funcname;  

void
memcheck_init()
{
    if ( isinit ) return;
    mem_dict = createHashTable(init_size);
    isinit = 1;
#ifdef MEMCHECK_MULTITHREAD
    if ( pthread_mutex_init(&big_lock, NULL) < 0 ) FATALERROR;
#endif 
}

void
memcheck_finalize()
{
#ifdef MEMCHECK_MULTITHREAD
    if ( pthread_mutex_lock(&big_lock) < 0 ) FATALERROR;
#endif
    if ( !isinit ) return;
    HashTableIter *iter = getHashTableIter(mem_dict);
    while ( hasNextElement(iter) ){
        TableEntry *e = nextElement(iter);
        free(e->value);
    }
    destroyHashTableIter(iter);
    destroyHashTable(mem_dict);
    isinit = 0;
#ifdef MEMCHECK_MULTITHREAD
    if ( pthread_mutex_unlock(&big_lock) < 0 ) FATALERROR;
    if ( pthread_mutex_destroy(&big_lock) < 0 ) FATALERROR;
#endif
}

static void
_memcheck_abnormal_detected(const char *msg,...)
{
    va_list va;
    va_start(va, msg);
    vfprintf(stderr, msg, va);
    exit(-1);
    va_end(va);
}

void*
memcheck_malloc_do(size_t sz, const char *filename, size_t lineno, const char *funcname)
{
#ifdef MEMCHECK_MULTITHREAD
    if ( pthread_mutex_lock(&big_lock) < 0 ) FATALERROR;
#endif
    MEMCHECK_UTILITY;
    souc->size = sz;

    void *res = malloc(sz);
    if ( !res ) FATALERROR;
    putKeyValue(mem_dict, res, souc);
#ifdef MEMCHECK_MULTITHREAD
    if ( pthread_mutex_unlock(&big_lock) < 0 ) FATALERROR;
#endif
    return res;
}


void*
memcheck_calloc_do(size_t cnt, size_t sz, const char *filename, size_t lineno, const char *funcname)
{
#ifdef MEMCHECK_MULTITHREAD
    if ( pthread_mutex_lock(&big_lock) < 0 ) FATALERROR;
#endif
    MEMCHECK_UTILITY;
    souc->size = cnt * sz;

    void *res = calloc(cnt, sz);
    if ( !res ) FATALERROR;
    putKeyValue(mem_dict, res, souc);
#ifdef MEMCHECK_MULTITHREAD
    if ( pthread_mutex_unlock(&big_lock) < 0 ) FATALERROR;
#endif
    return res;
}

void*
memcheck_realloc_do(void *ptr, size_t sz, const char *filename, size_t lineno, const char *funcname)
{
#ifdef MEMCHECK_MULTITHREAD
    if ( pthread_mutex_lock(&big_lock) < 0 ) FATALERROR;
#endif
    MEMCHECK_UTILITY;
    souc->size = sz;

    /* if ptr == NULL, realloc is just malloc */
    if( ptr ) {
        memcheck_source_t *old = findValueByKey(mem_dict, ptr);
        if ( !old ){
            _memcheck_abnormal_detected("memcheck:%s:%lu:%s: attempt to realloc not-malloced addr: %p\n", 
                    filename, lineno, funcname, ptr);
        }
        free(old);
        removeKey(mem_dict, ptr);
    }

    void *res = realloc(ptr, sz);
    if ( !res ) FATALERROR;

    putKeyValue(mem_dict, res, souc);
#ifdef MEMCHECK_MULTITHREAD
    if ( pthread_mutex_unlock(&big_lock) < 0 ) FATALERROR;
#endif
    return res;
}

void
memcheck_free_do(void *ptr, const char *filename, size_t lineno, const char *funcname)
{
#ifdef MEMCHECK_MULTITHREAD
    if ( pthread_mutex_lock(&big_lock) < 0 ) FATALERROR;
#endif
    /* if ptr == NULL, free do nothing */
    if ( ptr ){
        memcheck_source_t *old = findValueByKey(mem_dict, ptr);
        if ( !old ){
            _memcheck_abnormal_detected("memcheck:%s:%lu:%s: attempt to realloc not-malloced addr: %p\n", 
                    filename, lineno, funcname, ptr);
        }
        free(old);
        removeKey(mem_dict, ptr);
    }

    free(ptr);
#ifdef MEMCHECK_MULTITHREAD
    if ( pthread_mutex_unlock(&big_lock) < 0 ) FATALERROR;
#endif
}

void
memcheck_check_do()
{
#ifdef MEMCHECK_MULTITHREAD
    if ( pthread_mutex_lock(&big_lock) < 0 ) FATALERROR;
#endif
    printf("--- not freed:\n");

    HashTableIter *iter = getHashTableIter(mem_dict);
    while ( hasNextElement(iter) ){
        TableEntry *e = nextElement(iter);
        void *addr = e->key;
        memcheck_source_t *souc = e->value;
        printf("addr: %p, %lu bytes allocated at %s:%lu:%s\n", addr, souc->size, souc->filename, souc->lineno, souc->funcname);
    }
    destroyHashTableIter(iter);
#ifdef MEMCHECK_MULTITHREAD
    if ( pthread_mutex_unlock(&big_lock) < 0 ) FATALERROR;
#endif
}
