#include "threadpool.h"
#include "lock.h"
#include "fatalerror.h"
#include "memtools/memcheck.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

static void* _worker_run(void*);
static void* _manager_run(void*);

/* future utilities */

static future_list_t*
_future_list_create(size_t sz)
{
    future_list_t *fl = (future_list_t*) memcheck_malloc(sizeof(future_list_t));
    fl->size = sz;
    fl->entries = (future_list_entry_t*) memcheck_malloc(sizeof(future_list_entry_t) * sz);
    for ( size_t i = 0; i < sz; i++ ){
        fl->entries[i].fut_access = (cond_lock_t*) memcheck_malloc(sizeof(cond_lock_t));
        cond_lock_init(fl->entries[i].fut_access);
    }
    fl->list_pos = 0;
    fl->available_stack = (index_t*) memcheck_malloc(sizeof(index_t) * sz);
    fl->available_stack_pos = 0;
    return fl;
}

static void
_future_list_destroy(future_list_t *futs)
{
    for ( size_t i = 0; i < futs->size; i++ ){
        cond_lock_destroy(futs->entries[i].fut_access);
        memcheck_free(futs->entries[i].fut_access);
    }
    memcheck_free(futs->entries);
    memcheck_free(futs->available_stack);
    memcheck_free(futs);
}

static index_t 
_future_get_next(future_list_t *futs)
{
    if ( futs->available_stack_pos > 0 ){
        return futs->available_stack[ --futs->available_stack_pos ];
    }

    /* entries list is full */
    if ( futs->list_pos == futs->size ){
        futs->size *= 2;
        futs->entries = (future_list_entry_t*) memcheck_realloc(futs->entries, sizeof(future_list_entry_t) * futs->size);
        futs->available_stack = (index_t*) memcheck_realloc(futs->available_stack, sizeof(index_t) * futs->size);

        for ( size_t i = futs->size / 2; i < futs->size; i++ ){
            futs->entries[i].fut_access = (cond_lock_t*) memcheck_malloc(sizeof(cond_lock_t));
            cond_lock_init(futs->entries[i].fut_access);
        }
    }

    return futs->list_pos++;
}

static void
_future_put_available(future_list_t *futs, index_t ind)
{
    if ( futs->available_stack_pos == futs->size ){
        futs->available_stack_pos = 0;
        futs->list_pos = 0;
    }
    futs->available_stack[ futs->available_stack_pos++ ] = ind;
}

/* event utilities */

static event_queue_t*
_event_queue_create(size_t sz)
{
    event_queue_t *qu = (event_queue_t*) memcheck_malloc(sizeof(event_queue_t));
    qu->size = sz;
    qu->events = (manager_event_t*) memcheck_malloc(sizeof(manager_event_t) * sz);
    qu->head = 0;
    qu->tail = 0;
    return qu;
}

static void
_event_queue_destroy(event_queue_t *qu)
{
    memcheck_free(qu->events);
    memcheck_free(qu);
}

static void
_event_queue_push(event_queue_t *qu, manager_event_t *e)
{
    /* event_queue full */
    if ( (qu->tail + 1) % qu->size == qu->head ){
#ifdef DEBUG
        printf("doubling event_queue: old qu->size = %lu\n", qu->size);
#endif
        qu->events = (manager_event_t*) memcheck_realloc( qu->events, sizeof(manager_event_t) * qu->size * 2);
        if ( qu->head > qu->tail ){
            memcpy(qu->events + qu->size, qu->events, sizeof(manager_event_t) * qu->tail);
            qu->tail += qu->size;
        }
        qu->size *= 2;
// #ifdef DEBUG
//         printf("double event_queue done: qu->tail - qu->head = %ld\n", qu->tail - qu->head);
// #endif
    }
    qu->events[ qu->tail ] = *e;
    qu->tail = (qu->tail + 1) % qu->size;
}

static manager_event_t*
_event_queue_pop(event_queue_t *qu)
{
    if ( qu->head == qu->tail ) return NULL;

    manager_event_t *res = &qu->events[ qu->head ];
    qu->head = (qu->head + 1) % qu->size;
    return res;
}

/* task utilities */

static task_queue_t*
_task_queue_create(size_t sz)
{
    task_queue_t *qu = (task_queue_t*) memcheck_malloc(sizeof(task_queue_t));
    qu->size = sz;
    qu->tasks = (task_t*) memcheck_malloc(sizeof(task_t) * sz);
    qu->head = 0;
    qu->tail = 0;
    return qu;
}

static void
_task_queue_destroy(task_queue_t *qu)
{
    memcheck_free(qu->tasks);
    memcheck_free(qu);
}

static int
_task_queue_empty(task_queue_t *qu)
{
    return qu->head == qu->tail;
}

static void
_task_queue_push(task_queue_t *qu, task_t *t)
{
    /* task_queue full */
    if ( (qu->tail + 1) % qu->size == qu->head ){
#ifdef DEBUG
        printf("doubling task_queue: old qu->size = %lu\n", qu->size);
#endif
        qu->tasks = (task_t*) memcheck_realloc( qu->tasks, sizeof(task_t) * qu->size * 2);
        if ( qu->head > qu->tail ){
            memcpy(qu->tasks + qu->size, qu->tasks, sizeof(task_t) * qu->tail);
            qu->tail += qu->size;
        }
        qu->size *= 2;
// #ifdef DEBUG
//         printf("double task_queue done: qu->tail - qu->head = %ld\n", qu->tail - qu->head);
// #endif
    }
    qu->tasks[ qu->tail ] = *t;
    qu->tail = (qu->tail + 1) % qu->size;
}

static task_t*
_task_queue_pop(task_queue_t *qu)
{
    if ( qu->head == qu->tail ) return NULL;

    task_t *res = &qu->tasks[ qu->head ];
    qu->head = (qu->head + 1) % qu->size;
    return res;
}

/* threads communication */

static void
_inform_manager(threadpool_t *pool, manager_event_t *e)
{
    cond_lock_er_lock(&pool->manager_inform);
    _event_queue_push(pool->event_queue, e);
    cond_lock_er_activate(&pool->manager_inform);
}

static void
_manager_assign_task(threadpool_t *pool)
{
    while ( pool->pos > 0 ){
        task_t *t = _task_queue_pop(pool->task_queue);
        if ( t == NULL ) break;

        worker_t *wk = &pool->workers[ pool->worker_available_stack[--pool->pos] ];
        wk->task = *t;
        cond_lock_er_lock(&wk->worker_wakeup);
        cond_lock_er_activate(&wk->worker_wakeup);
    }
}

static int
_all_worker_available(threadpool_t *pool)
{
    return pool->size == pool->pos;
}

/* precondition: all worker available, no task*/
static void
_do_destroy_all(threadpool_t *pool)
{
    cond_lock_destroy(&pool->manager_inform);
    cond_lock_destroy(&pool->join);

    _event_queue_destroy(pool->event_queue);
    _task_queue_destroy(pool->task_queue);
    _future_list_destroy(pool->future_list);

    for ( size_t i = 0; i < pool->size; i++ ){
        worker_t *wk = &pool->workers[i];
        wk->task.task_type = task_die;
        cond_lock_er_lock(&wk->worker_wakeup);
        cond_lock_er_activate(&wk->worker_wakeup);
        if ( pthread_join(wk->worker, NULL) < 0 ) FATALERROR;

        cond_lock_destroy(&wk->worker_wakeup);
    }
    memcheck_free(pool->workers);
    memcheck_free(pool->worker_available_stack);
    memcheck_free(pool);
    pthread_exit(NULL);
}

static void
_manager_handle_event_call_die(threadpool_t *pool)
{
    /* empty all tasks */
    pool->state = threadpool_state_about_to_die;

    if ( _all_worker_available(pool) ) {
        assert( _task_queue_empty(pool->task_queue) );
        _do_destroy_all(pool);
    }
}

static void
_manager_handle_event_task_addin(threadpool_t *pool, task_t *t)
{
    switch (pool->state) {
        case threadpool_state_normal:
            cond_lock_er_lock(&pool->join);
            cond_lock_er_disactivate(&pool->join);
            _task_queue_push(pool->task_queue, t);
            _manager_assign_task(pool);
            break;
        case threadpool_state_about_to_die:
            break;
        default:
            assert(0);
    }
}

static void
_manager_handle_event_worker_done(threadpool_t *pool, index_t worker_ind)
{
    worker_t *wk = &pool->workers[worker_ind];
    task_t *t = &wk->task;
    if ( t->task_type == task_gofuture ) {
        future_list_entry_t *fe = &pool->future_list->entries[t->task_fut];
        fe->value = wk->worker_task_res;
        cond_lock_er_activate(fe->fut_access);
    }
    pool->worker_available_stack[ pool->pos++ ] = worker_ind;

    if ( _all_worker_available(pool) && _task_queue_empty(pool->task_queue) ){
        switch (pool->state){
            case threadpool_state_about_to_die:
                _do_destroy_all(pool);
                return;
            case threadpool_state_normal:
                cond_lock_er_lock(&pool->join);
                cond_lock_er_activate(&pool->join);
                break;
            default:
                assert(0);
        }
    } 
    _manager_assign_task(pool);
}

struct worker_args_s {
    threadpool_t    *pool;
    index_t         this_ind;
};

static void*
_manager_run(void *args)
{
    threadpool_t *pool = (threadpool_t*) args;
    for ( size_t i = 0; i < pool->size; i++ ){
        struct worker_args_s *worker_args = (struct worker_args_s*) memcheck_malloc(sizeof(struct worker_args_s));
        worker_args->pool = pool;
        worker_args->this_ind = i;
        if ( pthread_create(&pool->workers[i].worker, NULL, _worker_run, worker_args) < 0 ) FATALERROR;
    }

    for ( ; ; ){
        cond_lock_ee_wait(&pool->manager_inform);
        /* at least one inform to manager received */
        manager_event_t *e;
        while ( (e = _event_queue_pop(pool->event_queue)) != NULL ){
            switch ( e->event_type ){
                    break;
                case manager_event_call_die:
                    _manager_handle_event_call_die(pool);
                    break;
                case manager_event_task_addin:
                    _manager_handle_event_task_addin(pool, &e->data.task);
                    break;
                case manager_event_worker_done:
                    _manager_handle_event_worker_done(pool, e->data.worker_ind);
                    break;
                default:
                    assert(0);
            }
        }
        cond_lock_ee_finish(&pool->manager_inform);
    }
}

static void*
_worker_run(void *args)
{
    struct worker_args_s *real_args = (struct worker_args_s*) args;
    threadpool_t *pool = real_args->pool;
    index_t this_ind = real_args->this_ind;
    memcheck_free(args);
    worker_t *worker_self = &pool->workers[this_ind];

    for ( ; ; ){
        cond_lock_ee_wait(&worker_self->worker_wakeup);
        /* a new task is received */
        task_t *t = &worker_self->task;
        switch ( t->task_type ){
            case task_goroutine:
                t->task_func(t->task_argu);
                break;
            case task_gofuture:
                worker_self->worker_task_res = t->task_func(t->task_argu);
                break;
            case task_die:
                pthread_exit(NULL);
                break;
            default:
                assert(0);
        }
        manager_event_t e;
        e.event_type = manager_event_worker_done;
        e.data.worker_ind = this_ind;
        _inform_manager(pool, &e);
        cond_lock_ee_finish(&worker_self->worker_wakeup);
    }
    return NULL;
}

threadpool_t*
threadpool_create(size_t sz)
{
    if ( !sz ) return NULL;
    memcheck_init();

    threadpool_t *pool = (threadpool_t*) memcheck_malloc(sizeof(threadpool_t));
    /* manager itself at last */
    pool->state = threadpool_state_normal;
    cond_lock_init(&pool->manager_inform);
    cond_lock_init(&pool->join);
    
    pool->event_queue = _event_queue_create(sz + 2);
    pool->task_queue = _task_queue_create(sz + 2);
    pool->future_list = _future_list_create(sz);
    
    pool->size = sz;
    pool->workers = (worker_t*) memcheck_malloc(sizeof(worker_t) * sz);
    for ( size_t i = 0; i < sz; i++ ){
        worker_t *wk = &pool->workers[i];
        cond_lock_init(&wk->worker_wakeup);
    }
    pool->worker_available_stack = (index_t*) memcheck_malloc(sizeof(index_t) * sz);
    for ( size_t i = 0; i < sz; i++ ){
        pool->worker_available_stack[i] = sz - 1 - i;
    }
    pool->pos = sz;
    if ( pthread_create(&pool->manager, NULL, _manager_run, pool) < 0 ) FATALERROR;
    return pool;
}

/* immediately return */
/* the actual thread will terminate until all previous tasks done */
void
threadpool_destroy(threadpool_t *pool)
{
    manager_event_t e;
    e.event_type = manager_event_call_die;
    if ( pthread_detach(pool->manager) < 0 ) FATALERROR;
    _inform_manager(pool, &e);
}

void
threadpool_goroutine(threadpool_t *pool, void (*routine)(void*), void *args)
{
    manager_event_t e;
    e.event_type = manager_event_task_addin;
    e.data.task.task_type = task_goroutine;
    e.data.task.task_func = (void* (*)(void*))routine;
    e.data.task.task_argu = args;
    _inform_manager(pool, &e);
}

future_t 
threadpool_gofuture(threadpool_t *pool, void* (*routine)(void*), void *args)
{
    /* manager might be accessing a future_list_entry */
    /* a potential realloc will destroy it */
    cond_lock_lock(&pool->manager_inform);
    future_t fut = _future_get_next(pool->future_list);
    cond_lock_unlock(&pool->manager_inform);

    future_list_entry_t *fe = &pool->future_list->entries[fut];
    cond_lock_er_lock(fe->fut_access);
    manager_event_t e;
    e.event_type = manager_event_task_addin;
    e.data.task.task_type = task_gofuture;
    e.data.task.task_func = routine;
    e.data.task.task_argu = args;
    e.data.task.task_fut  = fut;
    _inform_manager(pool, &e);
    return fut;
}

void*
threadpool_get(threadpool_t *pool, future_t fut)
{
    void *res;
    future_list_entry_t *fe = &pool->future_list->entries[fut];
    cond_lock_ee_wait(fe->fut_access);
    res = fe->value;
    cond_lock_ee_finish(fe->fut_access);
    _future_put_available(pool->future_list, fut);
    return res;
}

void
threadpool_join(threadpool_t *pool)
{
    cond_lock_ee_wait(&pool->join);
    cond_lock_unlock(&pool->join);
}
