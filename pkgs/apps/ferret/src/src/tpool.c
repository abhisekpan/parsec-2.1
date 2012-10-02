/* AUTORIGHTS
Copyright (C) 2007 Princeton University
      
This file is part of Ferret Toolkit.

Ferret Toolkit is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/
/*
 * tpool.c: A thread pool
 *
 * Functions to start, manage and terminate a thread pool
 */

#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h> //Abhi
#include "tpool.h"



/* Creates a new thread pool.
 * After the function terminates successfully, nthreads threads are running.
 *
 * opts:        Array with the start function and thread arguments for each thread
 * nthreads:    Number of threads to create (and number of entries in opts)
 *
 * return:      Pointer to the thread pool (or NULL if an error occurred)
 */
//Abhi the last parameter indicates this is the last batch of threads to be created
tpool_t *tpool_create(tdesc_t *opts, int nthreads, int last) {
    int i;
    tpool_t *pool;
    const pthread_attr_t *attr;
    void *arg;
    int rv;
    //Abhi===
    static int numthreads;
    int ret, wait_over;
    //===
    /* Check arguments */
    if(opts == NULL || nthreads < 1) {
        return NULL;
    }
    for(i=0; i<nthreads; i++) {
        if(opts[i].start_routine == NULL) {
            return NULL;
        }
    }

    /* Create data structure */
    pool = (tpool_t *)malloc(sizeof(tpool_t));
    if(pool == NULL) {
        return NULL;
    }
    //Abhi===
    pool->threadargs = (ThreadArg_t *)malloc(sizeof(ThreadArg_t) * nthreads);
    if(pool->threadargs == NULL) {
        free(pool);
        return NULL;
    }
   
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * nthreads);
    if(pool->threads == NULL) {
        free(pool);
        return NULL;
    }
    
    //Abhi == initialize warm up barrier
#ifdef ENABLE_PARSEC_HOOKS
    ret = pthread_barrier_init(&warmup_barrier, NULL, nthreads + 1);
    if (ret != 0) {
      printf("Failed to initialize warmup barrier. Exiting...\n");
      exit(1);
    }
#endif
    /* Create threads and initialize data structures */
    //Abhi== 
    for(i=0; i<nthreads; i++) {
        if(opts[i].attr == NULL) {
            attr = NULL;
        } else {
            attr = opts[i].attr;
        }
        if(opts[i].arg == NULL) {
            //arg = NULL;
            pool->threadargs[i].arg = NULL;
        } else {
            //arg = opts[i].arg;
             pool->threadargs[i].arg = opts[i].arg;
        }
         pool->threadargs[i].tid = numthreads++;
        //Abhi
//        rv = pthread_create(&(pool->threads[i]), attr, opts[i].start_routine, arg);
        rv = pthread_create(&(pool->threads[i]), attr, opts[i].start_routine, (void*)&pool->threadargs[i]);
        if(rv != 0) {
            free(pool->threads);
            free(pool);
            return NULL;
        }
    }
    //==Abhi == barrier to indicate creation of threads
#ifdef ENABLE_PARSEC_HOOKS   
    wait_over = pthread_barrier_wait(&warmup_barrier);
    if (!((wait_over == PTHREAD_BARRIER_SERIAL_THREAD)|| (wait_over == 0))) {
      printf("Error in barrier wait. Exiting...\n");
      exit(1);
    } else {
      ret = pthread_barrier_destroy(&warmup_barrier);
      if (ret != 0) {
	printf("Failed to destroy warmup barrier. Exiting...\n");
	exit(1);
      }
      if (last==1) __parsec_warmup_over();
    }
#endif
    //====
    pool->nthreads = nthreads;
    pool->state = POOL_STATE_RUNNING;
    
    return pool;
}

/*
 * Destroys the thread pool.
 * The threads of the thread pool should already have been stopped when this function
 * is called.
 *
 * pool:        Pointer to the thread pool
 *
 */
void tpool_destroy(tpool_t *pool) {
    assert(pool!=NULL);
    assert(pool->state!=POOL_STATE_RUNNING);
    
    free(pool->threads);
    free(pool->threadargs); //Abhi
    free(pool);
}

/*
 * Waits until all threads have joined.
 *
 * pool:        Pointer to the thread pool
 * value_ptrs:  Array for the return values of the thread functions (can be NULL)
 *
 * return:      -1 if an error occurred, 0 otherwise
 */
int tpool_join(tpool_t *pool, void **value_ptrs) {
    int i;
    void **value_ptr;
    int rv;
    
    assert(pool!=NULL);
    assert(pool->state==POOL_STATE_RUNNING);

    /* Join threads */
    for(i=0; i<pool->nthreads; i++) {
        if(value_ptrs != NULL) {
            value_ptr = &(value_ptrs[i]);
        } else {
            value_ptr = NULL;
        }
        
        rv = pthread_join(pool->threads[i], value_ptr);
        if(rv != 0) {
            pool->state = POOL_STATE_ERROR;
            return -1;
        }
    }
    
    pool->state = POOL_STATE_READY;
    return 0;
}

/*
 * Cancels all threads of the pool.
 *
 * pool:        Pointer to the thread pool
 *
 * return:      -1 if an error occurred, 0 otherwise
 */
int tpool_cancel(tpool_t *pool) {
    int i;
    int rv;
    
    assert(pool!=NULL);
    assert(pool->state==POOL_STATE_RUNNING);

    rv = 0;    
    for(i=0; i<pool->nthreads; i++) {
        rv += pthread_cancel(pool->threads[i]);
    }
    
    if(rv != 0) {
        pool->state = POOL_STATE_ERROR;
        return -1;
    }
    
    pool->state = POOL_STATE_READY;
    return 0;
}
