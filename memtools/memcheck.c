#include "memcheck.h"
#include "hashtable_memcheck.h"
#include <stdlib.h>
#include <stddef.h>


HashTable *mem_dict;
const size_t init_size = 100;
int isinit;

void
memcheck_init()
{
    if ( isinit ) return;
    mem_dict = createHashTable(init_size);
    isinit = 1;
}

void
memcheck_finalize()
{
    if ( !isinit ) return;
    HashTableIter *iter = getHashTableIter(mem_dict);
    while ( hasNextElement(iter) ){
        TableEntry *e = nextElement(iter);
        free(e->value);
    }
    destroyHashTableIter(iter);
    destroyHashTable(mem_dict);
    isinit = 0;
}
