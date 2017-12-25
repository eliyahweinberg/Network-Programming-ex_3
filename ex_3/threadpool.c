//
//  threadpool.c
//  ex_3
//
//  Created by Eliyah Weinberg on 21.12.2017.
//  Copyright © 2017 Eliyah Weinberg. All rights reserved.
//

#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>

#define EMPTY 0
#define TRUE 1
#define FALSE 0


/**
 * create_threadpool creates a fixed-sized thread
 * pool.  If the function succeeds, it returns a (non-NULL)
 * "threadpool", else it returns NULL.
 */
threadpool* create_threadpool(int num_threads_in_pool){
    if (num_threads_in_pool > MAXT_IN_POOL || num_threads_in_pool < 1)
        return NULL;
    threadpool* tp = (threadpool*)malloc(sizeof(threadpool));
    tp->dont_accept = FALSE;
    tp->shutdown = FALSE;
    tp->num_threads = 0;
    tp->qsize = EMPTY;
    tp->qhead = NULL;
    tp->qtail = NULL;
    pthread_mutex_init(&tp->qlock, NULL);
    pthread_cond_init(&tp->q_empty, NULL);
    pthread_cond_init(&tp->q_not_empty, NULL);
    
    tp->threads = (pthread_t*)malloc(sizeof(pthread_t)*num_threads_in_pool);
    int i;
    for (i=0; i<num_threads_in_pool; i++)
        pthread_create(tp->threads+i, NULL, do_work, tp);
    
    return tp;
}


/**
 * dispatch enter a "job" of type work_t into the queue.
 * when an available thread takes a job from the queue, it will
 * call the function "dispatch_to_here" with argument "arg".
 * this function should:
 * 1. create and init work_t element
 * 2. lock the mutex
 * 3. add the work_t element to the queue
 * 4. unlock mutex
 *
 */
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg);

/**
 * The work function of the thread
 * this function should:
 * 1. lock mutex
 * 2. if the queue is empty, wait
 * 3. take the first element from the queue (work_t)
 * 4. unlock mutex
 * 5. call the thread routine
 *
 */
void* do_work(void* p){
    threadpool* tp = (threadpool*)p;
    work_t* new_work;
    
    while (TRUE) {
        if (tp->shutdown)
            break;
        
        /*Checking works in queue*/
        pthread_mutex_lock(&tp->qlock);
        if (tp->qsize == EMPTY){
            pthread_cond_wait(&tp->q_not_empty, &tp->qlock);
            
            if (tp->shutdown){
                pthread_mutex_unlock(&tp->qlock);
                break;
            }
        }
        /*taking new work from queue*/
       
        
        if (tp->dont_accept || tp->qsize == EMPTY)
            pthread_cond_signal(&tp->q_empty);
            
    }
    
    return NULL;
}


/**
 * destroy_threadpool kills the threadpool, causing
 * all threads in it to commit suicide, and then
 * frees all the memory associated with the threadpool.
 */
void destroy_threadpool(threadpool* destroyme){
    destroyme->dont_accept = TRUE; //refusing new works
    
    /*checking if work queue is empty */
    pthread_mutex_lock(&destroyme->qlock);
    if (destroyme->qsize != EMPTY)
        pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock);
    destroyme->shutdown = TRUE;
    pthread_cond_broadcast(&destroyme->q_not_empty);
    pthread_mutex_unlock(&destroyme->qlock);
    
    
    int i;
    for(i=0; i<destroyme->num_threads; i++)
        pthread_join(destroyme->threads[i], NULL);
    
    free(destroyme->threads);
    free(destroyme);
}




