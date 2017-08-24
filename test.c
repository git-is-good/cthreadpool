#include "threadpool.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

void routine(void *dumb){
    printf("hello: %lu\n", pthread_self());
}

void test(){
    threadpool_t *pool = threadpool_create(8);
    for ( int i = 0; i < 14; i++ )
        threadpool_goroutine(pool, routine, NULL);
    sleep(1);
}

int main(){
    test();
}

