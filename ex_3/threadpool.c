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

//#define P_DEBUG
//----------------------------------------------------------------------------//
//------------------------PRIVATE FUNCTION DECLARATION------------------------//
//----------------------------------------------------------------------------/
void db_print(char* msg);
//----------------------------------------------------------------------------//
//----------------------FUNCTIONS IMPLEMENTATION------------------------------//
//----------------------------------------------------------------------------//
/**
 * create_threadpool creates a fixed-sized thread
 * pool.  If the function succeeds, it returns a (non-NULL)
 * "threadpool", else it returns NULL */
threadpool* create_threadpool(int num_threads_in_pool){
    if (num_threads_in_pool > MAXT_IN_POOL || num_threads_in_pool < 1)
        return NULL;
    threadpool* pool = (threadpool*)malloc(sizeof(threadpool));
    if (!pool)
        return NULL;

    pool->dont_accept = FALSE;
    pool->shutdown = FALSE;
    pool->num_threads = num_threads_in_pool;
    pool->qsize = EMPTY;
    pool->qhead = NULL;
    pool->qtail = NULL;
    pthread_mutex_init(&pool->qlock, NULL);
    pthread_cond_init(&pool->q_empty, NULL);
    pthread_cond_init(&pool->q_not_empty, NULL);

    pool->threads = (pthread_t*)malloc(sizeof(pthread_t)*num_threads_in_pool);
    if (!pool->threads)
        return NULL;

    int i;
    for (i=0; i<num_threads_in_pool; i++)
        pthread_create(pool->threads+i, NULL, do_work, pool);

    return pool;
}


/**
 * dispatch enter a "job" of type work_t into the queue.
 * when an available thread takes a job from the queue, it will
 * call the function "dispatch_to_here" with argument "arg".
 */
void dispatch(threadpool* pool, dispatch_fn dispatch_to_here, void *arg){
    if (!pool)
        return;

    if (pool->dont_accept == TRUE)
        return;


    work_t* new_work = (work_t*)malloc(sizeof(work_t));
    if (!new_work)
        return;

    new_work->routine = dispatch_to_here;
    new_work->arg = arg;
    new_work->next = NULL;
    /*locking mutex for adding new work to the working queue*/
    pthread_mutex_lock(&pool->qlock);

    if(pool->qsize == EMPTY)
        pool->qhead = pool->qtail = new_work;
    else{
        pool->qtail->next = new_work;
        pool->qtail = new_work;
    }
    pool->qsize++;
    pthread_cond_signal(&pool->q_not_empty);
    pthread_mutex_unlock(&pool->qlock);
}

/**
 * The work function of the thread
 */
void* do_work(void* p){
    threadpool* pool = (threadpool*)p;
    work_t* new_work;

    while (TRUE) {
        pthread_mutex_lock(&pool->qlock);
        if (pool->shutdown == TRUE){
            pthread_mutex_unlock(&pool->qlock);
            break;
        }

        /*Checking if there is works in queue*/

        while (pool->qsize == EMPTY && pool->dont_accept == FALSE){
            pthread_cond_wait(&pool->q_not_empty, &pool->qlock);
            if (pool->shutdown == TRUE){
                pthread_mutex_unlock(&pool->qlock);
                return NULL;
            }
        }


        if (pool->qsize == EMPTY && pool->dont_accept == TRUE){
            pthread_cond_signal(&pool->q_empty);
            pthread_mutex_unlock(&pool->qlock);
            break;
        }

        /*taking new work from the queue*/
        new_work = pool->qhead;
        if (pool->qsize == 1)
            pool->qhead = pool->qtail = NULL;
        else
            pool->qhead = new_work->next;

        pool->qsize--;

        pthread_mutex_unlock(&pool->qlock);
        new_work->routine(new_work->arg);
        free(new_work);
    }

    return NULL;
}


/**
 * destroy_threadpool kills the threadpool, causing
 * all threads in it to commit suicide, and then
 * frees all the memory associated with the threadpool.
 */
void destroy_threadpool(threadpool* pool){
    if (!pool)
        return;
    pthread_mutex_lock(&pool->qlock);
    pool->dont_accept = TRUE; //refusing new works

    /*checking if work queue is empty */

    if (pool->qsize != EMPTY){
        pthread_cond_wait(&pool->q_empty, &pool->qlock);
    }
    pool->shutdown = TRUE;
    pthread_mutex_unlock(&pool->qlock);
    pthread_cond_broadcast(&pool->q_not_empty);


    int i;
    for(i=0; i<pool->num_threads; i++){
       pthread_join(pool->threads[i], NULL);
    }

    pthread_mutex_destroy(&pool->qlock);
    pthread_cond_destroy(&pool->q_empty);
    pthread_cond_destroy(&pool->q_not_empty);

    free(pool->threads);
    free(pool);
}

//----------------------------------------------------------------------------//
void db_print(char* msg){
#ifdef P_DEBUG
    printf("%s\n",msg);
#endif
}



