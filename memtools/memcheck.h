#ifndef _MEMCHECK_H_
#define _MEMCHECK_H_

#include "hashtable_memcheck.h"
#include "fatalerror.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

//TODO when call memcheck_check, display leaked memory in chronological order


/* for user use */
#define memcheck_malloc(size)           memcheck_malloc_do(size, __FILE__, __LINE__, __func__)
#define memcheck_calloc(count, size)    memcheck_calloc_do(count, size, __FILE__, __LINE__, __func__)
#define memcheck_realloc(ptr, size)     memcheck_realloc_do(ptr, size, __FILE__, __LINE__, __func__)
#define memcheck_free(ptr)              memcheck_free_do(ptr, __FILE__, __LINE__, __func__)
#define memcheck_check                  memcheck_check_do
/* user functions end */

void memcheck_init();
void memcheck_finalize();

void *memcheck_malloc_do(size_t sz, const char *filename, size_t lineno, const char *funcname);


void *memcheck_calloc_do(size_t cnt, size_t sz, const char *filename, size_t lineno, const char *funcname);

void *memcheck_realloc_do(void *ptr, size_t sz, const char *filename, size_t lineno, const char *funcname);

void memcheck_free_do(void *ptr, const char *filename, size_t lineno, const char *funcname);

void memcheck_check_do();

#endif /* _MEMCHECK_H_ */
