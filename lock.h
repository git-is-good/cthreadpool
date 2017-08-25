#ifndef _LOCK_H_
#define _LOCK_H_

#include "fatalerror.h"
#include <pthread.h>

typedef struct cond_lock_s {
    pthread_mutex_t     mut;
    pthread_cond_t      cond;
    int                 cond_ok;
} cond_lock_t;

static inline void 
cond_lock_init(cond_lock_t *cl)
{
    if ( pthread_mutex_init(&cl->mut, NULL) < 0 ) FATALERROR;
    if ( pthread_cond_init(&cl->cond, NULL) < 0 ) FATALERROR;
    cl->cond_ok = 0;
}

static inline void
cond_lock_destroy(cond_lock_t *cl)
{
    if ( pthread_mutex_destroy(&cl->mut) < 0 ) FATALERROR;
    if ( pthread_cond_destroy(&cl->cond) < 0 ) FATALERROR;
}

/* as normal lock */
static inline void
cond_lock_lock(cond_lock_t *cl)
{
    if ( pthread_mutex_lock(&cl->mut) < 0 ) FATALERROR;
}

static inline void
cond_lock_unlock(cond_lock_t *cl)
{
    if ( pthread_mutex_unlock(&cl->mut) < 0 ) FATALERROR;
}

/* ee is waiting some condition to continue */
static inline void
cond_lock_ee_wait(cond_lock_t *cl)
{
    if ( pthread_mutex_lock(&cl->mut) < 0 ) FATALERROR;
    while ( !cl->cond_ok ){
        if ( pthread_cond_wait(&cl->cond, &cl->mut) < 0 ) FATALERROR;
    }
}

static inline void
cond_lock_ee_finish(cond_lock_t *cl)
{
    cl->cond_ok = 0;
    if ( pthread_mutex_unlock(&cl->mut) < 0 ) FATALERROR;
}

/* er provides the condiditon */
static inline void 
cond_lock_er_lock(cond_lock_t *cl)
{
    if ( pthread_mutex_lock(&cl->mut) < 0 ) FATALERROR;
}

static inline void 
cond_lock_er_activate(cond_lock_t *cl)
{
    cl->cond_ok = 1;
    if ( pthread_mutex_unlock(&cl->mut) < 0 ) FATALERROR;
    if ( pthread_cond_signal(&cl->cond) < 0 ) FATALERROR;
}

static inline void
cond_lock_er_disactivate(cond_lock_t *cl)
{
    cl->cond_ok = 0;
    if ( pthread_mutex_unlock(&cl->mut) < 0 ) FATALERROR;
}

#endif /* _LOCK_H_ */
