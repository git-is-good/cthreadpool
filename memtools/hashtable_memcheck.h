#ifndef _HASHTABLE_MEMCHECK_H_
#define _HASHTABLE_MEMCHECK_H_

#include <stddef.h>
#include <stdlib.h>

typedef struct memcheck_source_s {
    const char      *filename;
    size_t          lineno;
    const char      *funcname;
    size_t          size;
} memcheck_source_t;

#define VALUETYPE               memcheck_source_t*
#define NONEXISTVALUE           NULL

// #define KEYTYPESTRING


// #ifdef KEYTYPESTRING
// 
// #define KEYTYPE                 char*
// #define NONEXISTKEY             NULL
// #define ISNONEXISTKEY(key)      ((key) == (KEYTYPE)NONEXISTKEY)
// #define ISEQUALKEY(key1, key2)  (strcmp((key1), (key2)) == 0)
// #define DEEPCOPYKEY(dstKey, srcKey) \
//           {\
//             dstKey = (char*) malloc(sizeof(char) * (strlen(srcKey) + 1)); \
//             strcpy(dstKey, key); \
//           }
// #define FREEKEY(key) free(key);
// 
// 
// 
// #else

#define KEYTYPE                       void*
#define NONEXISTKEY                   NULL
#define ISNONEXISTKEY(key)            ((key) == (KEYTYPE)NONEXISTKEY)
#define ISEQUALKEY(key1, key2)        ((key1) == (key2))
#define DEEPCOPYKEY(dstKey, srcKey)   (dstKey) = (KEYTYPE)(srcKey);
#define FREEKEY(key)

// #endif

typedef struct TableEntry {
    KEYTYPE key;
    VALUETYPE value;
    struct TableEntry *next;
} TableEntry;

typedef struct {
    size_t tableSize;
    size_t curSize;
    size_t (*hashFunc)(const KEYTYPE, size_t);
    struct TableEntry *contents;
} HashTable;

typedef struct HashTableIter {
    TableEntry *endPoint;
    TableEntry *curPoint;
    TableEntry *inBucketPoint;
} HashTableIter;

HashTable* createHashTable(size_t size);
void destroyHashTable(HashTable *tb);

void removeKey(HashTable *tb, const KEYTYPE key);
void putKeyValue(HashTable *tb, const KEYTYPE key, VALUETYPE value);

int hasKey(HashTable *tb, const KEYTYPE key);
TableEntry *findKey(HashTable *tb, const KEYTYPE key);
VALUETYPE findValueByKey(HashTable *tb, const KEYTYPE key);

size_t getHashTableSize(HashTable *tb);

HashTableIter *getHashTableIter(HashTable *tb);
void destroyHashTableIter(HashTableIter *ite);
int hasNextElement(HashTableIter *ite);
TableEntry *nextElement(HashTableIter *ite);

#endif /* _HASHTABLE_MEMCHECK_H_ */
