#include "memcheck.h"

void test(){
    memcheck_init();

    void* ptrs1[10];
    for ( int i = 0; i < 10; i++ ){
        ptrs1[i] = memcheck_malloc(12);
    }

    void* ptrs[12];
    for ( int i = 0; i < 12; i++ ){
        ptrs[i] = memcheck_calloc(13, 2);
    }

    for ( int i = 0; i < 12; i++ ){
        memcheck_free(ptrs[i]);
    }

    memcheck_check();

    for ( int i = 0; i < 10; i++ ){
        memcheck_free(ptrs1[i]);
    }

    memcheck_check();

    memcheck_finalize();

}

int main(){
    for ( long i = 0; i < 100000000; i++ ) test();
    //test();
}
