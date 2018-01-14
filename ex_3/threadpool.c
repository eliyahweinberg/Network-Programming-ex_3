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
    threadpool* tp = (threadpool*)malloc(sizeof(threadpool));
    if (!tp)
        return NULL;

    tp->dont_accept = FALSE;
    tp->shutdown = FALSE;
    tp->num_threads = num_threads_in_pool;
    tp->qsize = EMPTY;
    tp->qhead = NULL;
    tp->qtail = NULL;
    pthread_mutex_init(&tp->qlock, NULL);
    pthread_cond_init(&tp->q_empty, NULL);
    pthread_cond_init(&tp->q_not_empty, NULL);

    tp->threads = (pthread_t*)malloc(sizeof(pthread_t)*num_threads_in_pool);
    if (!tp->threads)
        return NULL;

    int i;
    for (i=0; i<num_threads_in_pool; i++)
        pthread_create(tp->threads+i, NULL, do_work, tp);

    return tp;
}


/**
 * dispatch enter a "job" of type work_t into the queue.
 * when an available thread takes a job from the queue, it will
 * call the function "dispatch_to_here" with argument "arg".
 */
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){
    if (!from_me)
        return;

    if (from_me->dont_accept == TRUE)
        return;


    work_t* new_work = (work_t*)malloc(sizeof(work_t));
    if (!new_work)
        return;

    new_work->routine = dispatch_to_here;
    new_work->arg = arg;
    new_work->next = NULL;
    /*locking mutex for adding new work to the working queue*/
    pthread_mutex_lock(&from_me->qlock);

    if(from_me->qsize == EMPTY)
        from_me->qhead = from_me->qtail = new_work;
    else{
        from_me->qtail->next = new_work;
        from_me->qtail = new_work;
    }
    from_me->qsize++;
    pthread_cond_signal(&from_me->q_not_empty);
    pthread_mutex_unlock(&from_me->qlock);
}

/**
 * The work function of the thread
 */
void* do_work(void* p){
    threadpool* tp = (threadpool*)p;
    work_t* new_work;
    db_print("do_work starts routine");

    while (TRUE) {
        pthread_mutex_lock(&tp->qlock);
        if (tp->shutdown == TRUE){
            pthread_mutex_unlock(&tp->qlock);
            db_print("at do_work unlocked mutex and shutdown");
            break;
        }

        /*Checking works in queue*/

        while (tp->qsize == EMPTY && tp->dont_accept == FALSE){
            db_print("at work wainting on q_not empty");
            pthread_cond_wait(&tp->q_not_empty, &tp->qlock);
            db_print("at work waked on q_not_empty");
            if (tp->shutdown == TRUE){
                db_print("at work waked and shut down");
                pthread_mutex_unlock(&tp->qlock);
                break;
            }
        }


        if (tp->qsize == EMPTY && tp->dont_accept == TRUE){
            db_print("at work sinal q_empty");
            pthread_cond_signal(&tp->q_empty);
            pthread_mutex_unlock(&tp->qlock);
            break;
        }

        /*taking new work from queue*/
        db_print("taking new work");
        new_work = tp->qhead;
        if (tp->qsize == 1)
            tp->qhead = tp->qtail = NULL;
        else
            tp->qhead = new_work->next;

        tp->qsize--;

        db_print("unlocking qlock");
        pthread_mutex_unlock(&tp->qlock);
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
void destroy_threadpool(threadpool* destroyme){
    if (!destroyme)
        return;
    db_print("destroy started trying to lock");
    pthread_mutex_lock(&destroyme->qlock);
    db_print("destory lock locked");
    destroyme->dont_accept = TRUE; //refusing new works

    /*checking if work queue is empty */

    if (destroyme->qsize != EMPTY){
        db_print("destroy waiting on q_empty");
        pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock);
    }
    destroyme->shutdown = TRUE;
    db_print("shutdown = True");
    pthread_cond_broadcast(&destroyme->q_not_empty);
    pthread_mutex_unlock(&destroyme->qlock);
    db_print("broadcast q_not_empty");


    int i;
    for(i=0; i<destroyme->num_threads; i++){
       pthread_join(destroyme->threads[i], NULL);
    }

    pthread_mutex_destroy(&destroyme->qlock);
    pthread_cond_destroy(&destroyme->q_empty);
    pthread_cond_destroy(&destroyme->q_not_empty);

    free(destroyme->threads);
    free(destroyme);
}

//----------------------------------------------------------------------------//
void db_print(char* msg){
#ifdef P_DEBUG
    printf("%s\n",msg);
#endif
}
//
//
//
//
//
//
//
//




