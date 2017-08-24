#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#include <pthread.h>
#include <stddef.h>

typedef int      future_t;
typedef size_t   index_t;

/* future utilities */

typedef enum {
    future_done,
    future_doing,
    future_not_present,
} future_state_t;

typedef struct future_list_entry_s {
    future_state_t      future_state;
    void*               value;
} future_list_entry_t;

typedef struct future_list_s{
    /* both entries size and available stack size */
    size_t              size;

    future_list_entry_t *entries;
    size_t              list_pos;

    /* all available indices */
    index_t             *available_stack;
    size_t              available_stack_pos;
} future_list_t;

/* task utilities */

typedef enum {
    task_goroutine,
    task_gofuture,
    task_die,
} task_type_t;

typedef struct task_s{
    task_type_t     task_type;
    void*           (*task_func)(void*);
    void*           task_argu;
    future_t        task_fut;
} task_t;

typedef struct task_queue_s {
    size_t size;
    task_t *tasks;
    index_t head;
    index_t tail;
} task_queue_t;

/* event_queue utilities */

typedef enum {
    manager_event_task_addin,
    manager_event_worker_done,
} manager_event_type_t;

typedef struct manager_event_s {
    manager_event_type_t event_type;
    union {
        task_t  task;
        index_t worker_ind;
    } data;
} manager_event_t;

typedef struct event_queue_s {
    size_t size;
    manager_event_t *events;
    index_t head;
    index_t tail;
} event_queue_t;

typedef struct worker_s {
    pthread_t           worker;
    pthread_mutex_t     worker_wakeup;
    pthread_cond_t      worker_wakeup_cond;
    int                 worker_wakeup_cond_ok;

    task_t              task;
    void*               worker_task_res;
} worker_t;


typedef struct threadpool_s {
    pthread_t           manager;
    pthread_mutex_t     manager_inform;
    pthread_cond_t      manager_inform_cond;
    int                 manager_inform_cond_ok;
    event_queue_t       *event_queue;

    task_queue_t        *task_queue;

    future_list_t       *future_list;

    size_t              size;
    worker_t            *workers;
    index_t             *worker_available_stack;
    size_t              pos;
} threadpool_t;

/* create and destroy */
threadpool_t *threadpool_create(size_t sz);
void threadpool_destroy(threadpool_t *pool);

/* run routine */
void threadpool_goroutine(threadpool_t *pool, void (*routine)(void*), void *args);

/* compute future result */
future_t threadpool_gofuture(threadpool_t *pool, void* (*routine)(void*), void *args);
void *threadpool_get(threadpool_t *pool, future_t fut);

#endif /* _THREADPOOL_H_ */
