#include "threadpool.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

static void* _worker_run(void*);
static void* _manager_run(void*);

/* future utilities */

static future_list_t*
_future_list_create(size_t sz)
{
    future_list_t *fl = (future_list_t*) malloc(sizeof(future_list_t));
    fl->size = sz;
    fl->entries = (future_list_entry_t*) malloc(sizeof(future_list_entry_t) * sz);
    for ( size_t i = 0; i < sz; i++ ){
        fl->entries[i].future_state = future_not_present;
    }
    fl->list_pos = 0;
    fl->available_stack = (index_t*) malloc(sizeof(index_t) * sz);
    fl->available_stack_pos = 0;
    return fl;
}

static void
_future_list_destroy(future_list_t *futs)
{
    free(futs->entries);
    free(futs->available_stack);
    free(futs);
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
        futs->entries = (future_list_entry_t*) realloc(futs->entries, sizeof(future_list_entry_t) * futs->size);
        futs->available_stack = (index_t*) realloc(futs->available_stack, sizeof(index_t) * futs->size);
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
    event_queue_t *qu = (event_queue_t*) malloc(sizeof(event_queue_t));
    qu->size = sz;
    qu->events = (manager_event_t*) malloc(sizeof(manager_event_t) * sz);
    qu->head = 0;
    qu->tail = 0;
    return qu;
}

static void
_event_queue_destroy(event_queue_t *qu)
{
    free(qu->events);
    free(qu);
}

static void
_event_queue_push(event_queue_t *qu, manager_event_t *e)
{
    /* event_queue full */
    if ( (qu->tail + 1) % qu->size == qu->head ){
        qu->events = (manager_event_t*) realloc( qu->events, sizeof(manager_event_t) * qu->size * 2);
        if ( qu->head > qu->tail ){
            memcpy(qu->events + qu->size, qu->events, sizeof(manager_event_t) * qu->tail);
            qu->tail = qu->head + 1 + qu->tail;
        }
        qu->size *= 2;
    }
    qu->events[ qu->tail ].event_type = e->event_type;
    qu->events[ qu->tail ].data = e->data;
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
    task_queue_t *qu = (task_queue_t*) malloc(sizeof(task_queue_t));
    qu->size = sz;
    qu->tasks = (task_t*) malloc(sizeof(task_t) * sz);
    qu->head = 0;
    qu->tail = 0;
    return qu;
}

static void
_task_queue_destroy(task_queue_t *qu)
{
    free(qu->tasks);
    free(qu);
}

static void
_task_queue_push(task_queue_t *qu, task_t *t)
{
    /* task_queue full */
    if ( (qu->tail + 1) % qu->size == qu->head ){
        qu->tasks = (task_t*) realloc( qu->tasks, sizeof(task_t) * qu->size * 2);
        if ( qu->head > qu->tail ){
            memcpy(qu->tasks + qu->size, qu->tasks, sizeof(task_t) * qu->tail);
            qu->tail = qu->head + 1 + qu->tail;
        }
        qu->size *= 2;
    }
    qu->tasks[ qu->tail ].task_type = t->task_type;
    qu->tasks[ qu->tail ].task_func = t->task_func;
    qu->tasks[ qu->tail ].task_argu = t->task_argu;
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

static void
_inform_manager(threadpool_t *pool, manager_event_t *e)
{
    pthread_mutex_lock(&pool->manager_inform);
    _event_queue_push(pool->event_queue, e);
    pool->manager_inform_cond_ok = 1;
    pthread_mutex_unlock(&pool->manager_inform);
    pthread_cond_signal(&pool->manager_inform_cond);
}

static void
_manager_assign_task(threadpool_t *pool)
{
    while ( pool->pos > 0 ){
        task_t *t = _task_queue_pop(pool->task_queue);
        if ( t == NULL ) break;

        worker_t *wk = &pool->workers[ pool->worker_available_stack[--pool->pos] ];
        wk->task = *t;
        pthread_mutex_lock(&wk->worker_wakeup);
        wk->worker_wakeup_cond_ok = 1;
        pthread_mutex_unlock(&wk->worker_wakeup);
        pthread_cond_signal(&wk->worker_wakeup_cond);
    }
}

static void
_manager_handle_event_task_addin(threadpool_t *pool, task_t *t)
{
    _task_queue_push(pool->task_queue, t);
    _manager_assign_task(pool);
}

static void
_manager_handle_event_worker_done(threadpool_t *pool, index_t worker_ind)
{
    worker_t *wk = &pool->workers[worker_ind];
    task_t *t = &wk->task;
    if ( t->task_type == task_gofuture ) {
        future_list_entry_t *fe = &pool->future_list->entries[t->task_fut];
        assert( fe->future_state == future_doing );
        fe->future_state = future_done;
        fe->value = wk->worker_task_res;
    }
    pool->worker_available_stack[ pool->pos++ ] = worker_ind;
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
        struct worker_args_s *worker_args = (struct worker_args_s*) malloc(sizeof(struct worker_args_s));
        worker_args->pool = pool;
        worker_args->this_ind = i;
        pthread_create(&pool->workers[i].worker, NULL, _worker_run, worker_args);
    }

    pthread_mutex_lock(&pool->manager_inform);
    for ( ; ; ){
        while ( !pool->manager_inform_cond_ok ){
            pthread_cond_wait(&pool->manager_inform_cond, &pool->manager_inform);
        }
        pool->manager_inform_cond_ok = 0;
        
        /* at least one inform to manager received */
        manager_event_t *e;
        while ( (e = _event_queue_pop(pool->event_queue)) != NULL ){
            switch ( e->event_type ){
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
    }
}

static void*
_worker_run(void *args)
{
    struct worker_args_s *real_args = (struct worker_args_s*) args;
    threadpool_t *pool = real_args->pool;
    index_t this_ind = real_args->this_ind;
    free(args);
    worker_t *worker_self = &pool->workers[this_ind];

    pthread_mutex_lock(&worker_self->worker_wakeup);
    for ( ; ; ){
        while ( !worker_self->worker_wakeup_cond_ok ){
            pthread_cond_wait(&worker_self->worker_wakeup_cond, &worker_self->worker_wakeup);
        }
        worker_self->worker_wakeup_cond_ok = 0;
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
                return NULL;
            default:
                assert(0);
        }
        manager_event_t e;
        e.event_type = manager_event_worker_done;
        e.data.worker_ind = this_ind;
        _inform_manager(pool, &e);
    }
    return NULL;
}

threadpool_t*
threadpool_create(size_t sz)
{
    if ( !sz ) return NULL;

    threadpool_t *pool = (threadpool_t*) malloc(sizeof(threadpool_t));
    /* manager itself at last */
    pthread_mutex_init(&pool->manager_inform, NULL);
    pthread_cond_init(&pool->manager_inform_cond, NULL);
    pool->manager_inform_cond_ok = 0;
    
    pool->event_queue = _event_queue_create(sz);
    pool->task_queue = _task_queue_create(sz);
    pool->future_list = _future_list_create(sz);
    
    pool->size = sz;
    pool->workers = (worker_t*) malloc(sizeof(worker_t) * sz);
    for ( size_t i = 0; i < sz; i++ ){
        worker_t *wk = &pool->workers[i];
        pthread_mutex_init(&wk->worker_wakeup, NULL);
        pthread_cond_init(&wk->worker_wakeup_cond, NULL);
        wk->worker_wakeup_cond_ok = 0;
    }
    pool->worker_available_stack = (index_t*) malloc(sizeof(index_t) * sz);
    for ( size_t i = 0; i < sz; i++ ){
        pool->worker_available_stack[i] = sz - 1 - i;
    }
    pool->pos = sz;
    pthread_create(&pool->manager, NULL, _manager_run, pool);
    return pool;
}

void
threadpool_destroy(threadpool_t *pool)
{

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
    future_t fut = _future_get_next(pool->future_list);
    pool->future_list->entries[fut].future_state = future_doing;
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

    return NULL;
}

