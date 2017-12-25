//
//  threadpool.h
//  ex_3
//
//  Created by Eliyah Weinberg on 21.12.2017.
//  Copyright © 2017 Eliyah Weinberg. All rights reserved.
//

#ifndef threadpool_h
#define threadpool_h

#include <pthread.h>

// maximum number of threads allowed in a pool
#define MAXT_IN_POOL 200


/**
 * the pool holds a queue of this structure
 */
typedef struct work_st{
    int (*routine) (void*);  //the threads process function
    void * arg;  //argument to the function
    struct work_st* next;
} work_t;


/**
 * The actual pool
 */
typedef struct _threadpool_st {
    int num_threads;    //number of active threads
    int qsize;            //number in the queue
    pthread_t *threads;    //pointer to threads
    work_t* qhead;        //queue head pointer
    work_t* qtail;        //queue tail pointer
    pthread_mutex_t qlock;        //lock on the queue list
    pthread_cond_t q_not_empty;    //non empty and empty condidtion vairiables
    pthread_cond_t q_empty;
    int shutdown;            //1 if the pool is in distruction process
    int dont_accept;       //1 if destroy function has begun
} threadpool;


// "dispatch_fn" declares a typed function pointer.  A
// variable of type "dispatch_fn" points to a function
// with the following signature:
//
//     int dispatch_function(void *arg);

typedef int (*dispatch_fn)(void *);

/**
 * create_threadpool creates a fixed-sized thread
 * pool.  If the function succeeds, it returns a (non-NULL)
 * "threadpool", else it returns NULL.
 */
threadpool* create_threadpool(int num_threads_in_pool);


/**
 * dispatch enter a "job" of type work_t into the queue.
 * when an available thread takes a job from the queue, it will
 * call the function "dispatch_to_here" with argument "arg".
 */
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg);

/**
 * The work function of the thread
 */
void* do_work(void* p);


/**
 * destroy_threadpool kills the threadpool, causing
 * all threads in it to commit suicide, and then
 * frees all the memory associated with the threadpool.
 */
void destroy_threadpool(threadpool* destroyme);



#endif /* threadpool_h */
