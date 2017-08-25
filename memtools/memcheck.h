#ifndef _MEMCHECK_H_
#define _MEMCHECK_H_

#include "hashtable_memcheck.h"
#include "fatalerror.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

/* for user use */
#define memcheck_malloc(size)           memcheck_malloc_do(size, __FILE__, __LINE__, __func__)
#define memcheck_calloc(count, size)    memcheck_calloc_do(count, size, __FILE__, __LINE__, __func__)
#define memcheck_realloc(ptr, size)     memcheck_realloc_do(ptr, size, __FILE__, __LINE__, __func__)
#define memcheck_free(ptr)              memcheck_free_do(ptr, __FILE__, __LINE__, __func__)
#define memcheck_check                  memcheck_check_do
/* user functions end */


#define MEMCHECK_UTILITY        \
    memcheck_source_t *souc = (memcheck_source_t*) malloc(sizeof(memcheck_source_t)); \
    souc->filename = filename;  \
    souc->lineno = lineno;      \
    souc->funcname = funcname;  

extern HashTable *mem_dict;

static inline void
_memcheck_abnormal_detected(const char *msg,...)
{
    va_list va;
    va_start(va, msg);
    vprintf(msg, va);
    exit(-1);
    va_end(va);
}

void memcheck_init();
void memcheck_finalize();

static inline void*
memcheck_malloc_do(size_t sz, const char *filename, size_t lineno, const char *funcname)
{
    MEMCHECK_UTILITY;
    souc->size = sz;

    void *res = malloc(sz);
    if ( !res ) FATALERROR;
    putKeyValue(mem_dict, res, souc);
    return res;
}


static inline void*
memcheck_calloc_do(size_t cnt, size_t sz, const char *filename, size_t lineno, const char *funcname)
{
    MEMCHECK_UTILITY;
    souc->size = cnt * sz;

    void *res = calloc(cnt, sz);
    if ( !res ) FATALERROR;
    putKeyValue(mem_dict, res, souc);
    return res;
}

static inline void*
memcheck_realloc_do(void *ptr, size_t sz, const char *filename, size_t lineno, const char *funcname)
{
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
    return res;
}

static inline void
memcheck_free_do(void *ptr, const char *filename, size_t lineno, const char *funcname)
{
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
}

static inline void
memcheck_check_do()
{
    printf("--- not freed:\n");

    HashTableIter *iter = getHashTableIter(mem_dict);
    while ( hasNextElement(iter) ){
        TableEntry *e = nextElement(iter);
        void *addr = e->key;
        memcheck_source_t *souc = e->value;
        printf("addr: %p, %lu bytes allocated at %s:%lu:%s\n", addr, souc->size, souc->filename, souc->lineno, souc->funcname);
    }
    destroyHashTableIter(iter);
}

#endif /* _MEMCHECK_H_ */
