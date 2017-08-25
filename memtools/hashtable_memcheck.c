#include "hashtable_memcheck.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// This is a hashtable hashing a string to a function pointer

// The implementation is based on these considerations:
// 1, If we keep rehashing when a threshold point is reached, then almost all elements are stored in a bucket with only one element

//#define DEBUG_HASHTABLE

static const double rehashThreshold  = 0.75;
static const unsigned int rehashRate = 2;

static void _rehash(HashTable *tb);

static size_t
defaultHashFunc(const KEYTYPE key, size_t tableSize)
{
#ifdef KEYTYPESTRING
    size_t sum = 0;
    while ( *key ){
        sum = 37 * sum + *key++;
    }
    return sum % tableSize;
#else
    return (size_t)key % tableSize;
#endif
}

HashTable*
createHashTable(size_t size)
{
    // defensive
    if ( size == 0 ) size = 1;
    HashTable* tb = (HashTable*) malloc(sizeof(HashTable));
    tb->tableSize = size;
    tb->curSize = 0;
    tb->hashFunc = defaultHashFunc;
// #if defined KEYTYPESTRING && NONEXISTKEY == NULL
//     tb->contents = (TableEntry*) calloc(size, sizeof(TableEntry));
// #else
    tb->contents = (TableEntry*) malloc(sizeof(TableEntry) * size);
    for ( size_t i = 0; i < size; i++ ){
        tb->contents[i].key = (KEYTYPE)NONEXISTKEY;
        tb->contents[i].next = NULL;
    }
// #endif
    return tb;
}

static void
_destroyHashTable(HashTable *tb, int destroyKey)
{
    for ( size_t i = 0; i < tb->tableSize; i++ ){
        if ( ISNONEXISTKEY(tb->contents[i].key) ) continue;
        
        // destroy the whole linked list
        if ( destroyKey ) FREEKEY( tb->contents[i].key );
        TableEntry *te = tb->contents[i].next;
        while ( te ){
            TableEntry *teNext = te->next;
            if ( destroyKey ) FREEKEY(te->key);
            free(te);
            te = teNext;
        }
    }
    free(tb->contents);
}

void
destroyHashTable(HashTable *tb){
    _destroyHashTable(tb, 1);
    free(tb);
}

// if this key doesn't exist, we do nothing and just return 
void
removeKey(HashTable *tb, const KEYTYPE key)
{
    struct TableEntry* thisBucket = &tb->contents[tb->hashFunc(key, tb->tableSize)];
    
    // it's the head ?
    if ( ISNONEXISTKEY(thisBucket->key) ) return;
    if ( ISEQUALKEY(thisBucket->key, key) ){
        FREEKEY(thisBucket->key);
        if ( thisBucket->next ) {
            TableEntry *nextOne = thisBucket->next;
            thisBucket->key = nextOne->key;
            thisBucket->value = nextOne->value;
            thisBucket->next = nextOne->next;
            free(nextOne);
        }else{
            thisBucket->key = (KEYTYPE)NONEXISTKEY;
        }
        tb->curSize--;
        return;
    }

    struct TableEntry *prev = thisBucket, *cur = thisBucket->next;
    while ( cur ){
        if ( ISEQUALKEY(cur->key, key) ){
            // found
            prev->next = cur->next;
            FREEKEY(cur->key);
            free(cur);
            tb->curSize--;
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

static struct TableEntry*
_createTableEntry( const KEYTYPE key, VALUETYPE value, int copyKey )
{
    struct TableEntry *res = (struct TableEntry*) malloc(sizeof(struct TableEntry));
    if ( copyKey ){
        DEEPCOPYKEY(res->key, key);
    }else{
        res->key = (KEYTYPE)key;
    }
    res->value = value;
    res->next = NULL;
    return res;
}

static void
_putTableEntry(HashTable *tb, TableEntry *te)
{
    TableEntry* thisBucket = &tb->contents[tb->hashFunc(te->key, tb->tableSize)];
    
    if ( ISNONEXISTKEY(thisBucket->key) ){
        thisBucket->key = te->key;
        thisBucket->value = te->value;
        free(te);
        assert(thisBucket->next == NULL);
        return;
    }
    te->next = thisBucket->next;
    thisBucket->next = te;
}

// if this key already exists, then just update its value
static void
_putKeyValue(HashTable *tb, const KEYTYPE key, VALUETYPE value, int copyKey)
{
    struct TableEntry* thisBucket = &tb->contents[tb->hashFunc(key, tb->tableSize)];
    
    // handle the head
    if ( ISNONEXISTKEY(thisBucket->key) ){
        // new key
        if ( copyKey ){
            DEEPCOPYKEY(thisBucket->key, key);
        }else{
            thisBucket->key = (KEYTYPE)key;
        }
        thisBucket->value = value;
        if ( ++tb->curSize > rehashThreshold * tb->tableSize ) _rehash(tb);
        return;
    }
    if ( ISEQUALKEY(thisBucket->key, key) ){
        // update
        thisBucket->value = value;
        return;
    }

    struct TableEntry *prev = thisBucket, *cur = thisBucket->next;
    while ( cur ){
        // update
        if ( ISEQUALKEY(cur->key, key) ){
            cur->value = value;
            return;
        }
        prev = cur;
        cur = cur->next;
    }

    // a new key
    prev->next = _createTableEntry(key, value, copyKey);
    if ( ++tb->curSize > rehashThreshold * tb->tableSize ) _rehash(tb);
}

void
putKeyValue(HashTable *tb, const KEYTYPE key, VALUETYPE value)
{
    _putKeyValue(tb, key, value, 1);
}

TableEntry*
findKey(HashTable *tb, const KEYTYPE key)
{
    struct TableEntry *thisBucket = &tb->contents[tb->hashFunc(key, tb->tableSize)],
    *walker = thisBucket->next;
    if ( !ISNONEXISTKEY(thisBucket->key) && ISEQUALKEY(thisBucket->key, key) ) return thisBucket;
    
    while ( walker ){
        if ( ISEQUALKEY(walker->key, key) ) return walker;
        walker = walker->next;
    }
    return NULL;
}

int
hasKey(HashTable *tb, const KEYTYPE key)
{
    return findKey(tb, key) != NULL;
}

VALUETYPE
findValueByKey(HashTable *tb, const KEYTYPE key)
{
    TableEntry* te = findKey(tb, key);
    if ( te ) return te->value;
    return (VALUETYPE)NONEXISTVALUE;
}

static void
_rehash(HashTable *tb)
{
    HashTable *newtb = createHashTable(tb->tableSize * rehashRate);
    for ( size_t i = 0; i < tb->tableSize; i++ ){
        TableEntry *thisBucket = &tb->contents[i];
        
        // the head TableEntry cannot be put to other place
        if ( ISNONEXISTKEY(thisBucket->key) ) continue;
        _putKeyValue(newtb, thisBucket->key, thisBucket->value, 0);
        
        TableEntry *cur = thisBucket->next;
        while (cur){
            TableEntry *tmp = cur->next;
            _putTableEntry(newtb, cur);
            cur = tmp;
        }
    }
    
    // now all memory allocated by the lifecycle of tb is tb->contents
    free(tb->contents);
    tb->tableSize = newtb->tableSize;
    tb->contents = newtb->contents;
    free(newtb);
}

static void
_gotoNext(HashTableIter *ite)
{
    if ( ite->curPoint == ite->endPoint ) return;
    assert(ite->inBucketPoint);
    
    ite->inBucketPoint = ite->inBucketPoint->next;
    if ( ite->inBucketPoint ) return;
    
    while ( ++ite->curPoint != ite->endPoint && ISNONEXISTKEY(ite->curPoint->key) )
        ;
    if ( ite->curPoint != ite->endPoint ){
        ite->inBucketPoint = ite->curPoint;
    }else{
        ite->inBucketPoint = NULL;
    }
    return;
}

HashTableIter*
getHashTableIter(HashTable *tb)
{
    HashTableIter *ite = (HashTableIter*) malloc(sizeof(HashTableIter));
    ite->endPoint = tb->contents + tb->tableSize;
    ite->curPoint = tb->contents;
    while ( ite->curPoint != ite->endPoint && ISNONEXISTKEY(ite->curPoint->key) ){
        ite->curPoint++;
    }
    if ( ite->curPoint != ite->endPoint ){
        ite->inBucketPoint = ite->curPoint;
    }else{
        ite->inBucketPoint = NULL;
    }
    return ite;
}

void
destroyHashTableIter(HashTableIter *ite)
{
    free(ite);
}

int
hasNextElement(HashTableIter *ite)
{
    return ite->curPoint != ite->endPoint;
}

TableEntry*
nextElement(HashTableIter *ite)
{
    if ( ite->curPoint == ite->endPoint ) return NULL;
    TableEntry *res = ite->curPoint;
    _gotoNext(ite);
    return res;
}

size_t
getHashTableSize(HashTable *tb)
{
    return tb->curSize;
}


#ifdef DEBUG_HASHTABLE
static void
printHashTable(HashTable *tb)
{
    printf("HashTable: size = %d, curSize = %d\n", tb->tableSize, tb->curSize);
    for ( size_t i = 0; i < tb->tableSize; i++ ){
        struct TableEntry *thisBucket = &tb->contents[i];
        if ( thisBucket->key ){
            printf("           hashcode: %d\n", i);
            struct TableEntry *cur = thisBucket;
            while (cur){
                printf("             key = \"%s\", value = %p\n", cur->key, cur->value);
                cur = cur->next;
            }
        }
    }
}

static void
putAndShowPutMsg(HashTable *tb, const char *s, void* p)
{
    printf("### putting \"%s\", %p ...\n", s, p);
    putKeyValue(tb, s, p);
}

static void
removeAndShow(HashTable *tb, const char *s)
{
    printf("### removing \"%s\" ...\n", s);
    removeKey(tb, s);
}

void
test()
{
    HashTable *tb = createHashTable(1);
    void *ptr1, *ptr2;

    putAndShowPutMsg(tb, "hello world", (void*)0x101);
    ptr1 = findValueByKey(tb, "hello world");
    assert(ptr1 == (void*)0x101);
    printHashTable(tb);

    putAndShowPutMsg(tb, "goodmorning", (void*)0x932);
    printHashTable(tb);

    putAndShowPutMsg(tb, "this is a good day", (void*)0x9999);
    printHashTable(tb);

    putAndShowPutMsg(tb, "writing", (void*)0x8888);
    printHashTable(tb);
    
    putAndShowPutMsg(tb, "falling", (void*)0x777);
    printHashTable(tb);

    removeAndShow(tb, "goodmorning");
    removeAndShow(tb, "hello world");
    printHashTable(tb);

    putAndShowPutMsg(tb, "hello world", (void*)0x1221432);
    printHashTable(tb);
    
    putAndShowPutMsg(tb, "falling", (void*)0x23412343);
    printHashTable(tb);
    assert( findValueByKey(tb, "falling") == (void*)0x23412343 );
    assert( findValueByKey(tb, "goodmorning") == NULL );
    
    putAndShowPutMsg(tb, "goodmorning", (void*)0x9876);
    assert( findValueByKey(tb, "goodmorning") == (void*)0x9876);

    putAndShowPutMsg(tb, "cpp", (void*)0x123);
    putAndShowPutMsg(tb, "java", (void*)0x1252);
    printHashTable(tb);
    
    HashTableIter *iter = getHashTableIter(tb);
    while ( hasNextElement(iter) ){
        TableEntry *te = nextElement(iter);
        printf("*** key = \"%s\", value = %p\n", te->key, te->value);
    }
    destroyHashTableIter(iter);

    destroyHashTable(tb);
    printf("curSize: %d\n", getHashTableSize(tb));
    printf("done...\n");
}

int
main()
{
    test();
}
#endif
