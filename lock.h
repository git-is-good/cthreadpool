#ifndef _LOCK_H_
#define _LOCK_H_

#include <pthread.h>

typedef cond_lock_s {
    pthread_mutex_t     mut;
    pthread_cond_t      cond;
    int                 cond_ok;
} cond_lock_t;

static void 
cond_lock_init(cond_lock_t *cl)
{
    pthread_mutex_init(&cl->mut, NULL);
    pthread_cond_init(&cl->cond, NULL);
    cl->cond_ok = 0;
}

static void
cond_lock_destroy(cond_lock_t *cl)
{
    pthread_mutex_destroy(&cl->mut);
    pthread_cond_destroy(&cl->cond);
}

static void
cond_lock_ee_wait(cond_lock_t *cl)
{
    pthread_mutex_lock(&cl->mut);
    while ( !cl->cond_ok ){
        pthread_cond_wait(&cl->cond, &cl->mut);
    }
}

static void
cond_lock_ee_finish(cond_lock_t *cl)
{
    cl->cond_ok = 0;
    pthread_mutex_unlock(&cl->mut);
}

static void 
cond_lock_er_lock(cond_lock_t *cl)
{
    pthread_mutex_lock(&cl->mut);
}

static void 
cond_lock_er_activate(cond_lock_t *cl)
{
    cl->cond_ok = 1;
    pthread_mutex_unlock(&cl->mut);
    pthread_cond_signal(&cl->cond);
}


#endif /* _LOCK_H_ */
