#include "threadpool.h"
#include "memtools/memcheck.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

void routine(void *dumb){
    printf("hello: %lu\n", pthread_self());
}

void evil(void *dumb){
    printf("evil sleep@ %lu: %lu\n", pthread_self(), (unsigned long)dumb);
//    sleep(1);//(unsigned long)dumb);
}

void *futroutine(void *dumb) { return (void*)((long)dumb + 1); }
    

void testbasic(){
    threadpool_t *pool = threadpool_create(8);
#define SZ 15000
    for ( int i = 0; i < SZ; i++ )
        threadpool_goroutine(pool, routine, NULL);

    for ( long i = 0; i < SZ; i++ ){
        threadpool_goroutine(pool, evil, (void*)i );
    }

    future_t futs[SZ];
    for ( long i = 0; i < SZ; i++ ){
        futs[i] = threadpool_gofuture(pool, futroutine, (void*)i);
    }
    for ( long i = 0; i < SZ; i++ ){
        printf("res[%lu] = %lu\n", i, (unsigned long)threadpool_get(pool, futs[i]));
    }

    threadpool_join(pool);
    threadpool_destroy(pool);
}

void test_create_leak(){
    for ( size_t i = 0; i < 10; i++ ){
        threadpool_t *pool = threadpool_create(100);
        sleep(1);
        printf("i = %lu\n", i);
        //threadpool_t *pool = threadpool_create(10);
        threadpool_destroy(pool);
    }
}

int main(){
    testbasic();
//    test_create_leak();    
    sleep(5);
    memcheck_check();
    printf("main thread about to terminate\n");
    _exit(0);
    printf("never print\n");
}

