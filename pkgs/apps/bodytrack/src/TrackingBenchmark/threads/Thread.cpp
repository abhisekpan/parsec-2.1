// (C) Copyright Christian Bienia 2007
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0.
//
//  file : Thread.cpp
//  author : Christian Bienia - cbienia@cs.princeton.edu
//  description : A C++ thread

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#if defined(HAVE_LIBPTHREAD)
# include <pthread.h>
#else //default: winthreads
# include <windows.h>
# include <process.h>
#endif //HAVE_LIBPTHREAD

#include <typeinfo>
#include "Thread.h"
//Abhi===
#ifdef ENABLE_PARSEC_HOOKS
#include <hooks.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#define __PARSEC_CPU_BASE "PARSEC_CPU_BASE"
#define __PARSEC_CPU_NUM "PARSEC_CPU_NUM"
#endif


namespace threads {
int Thread::thread_count = 0;
Mutex* Thread::M = new Mutex;
//=======

#if defined(HAVE_LIBPTHREAD)
// Unfortunately, thread libraries such as pthreads which use the C
// calling convention are incompatible with C++ member functions.
// To provide an object-oriented thread interface despite this obstacle,
// we make use of a helper function which will wrap the member function.
extern "C" {
  static void *thread_entry(void *arg) {
    //Abhi ==bind the threads to a core
    //Runnable *tobj = static_cast<Runnable *>(arg);
    ThreadArg *threadarg = static_cast<ThreadArg *>(arg);
    Runnable *tobj = threadarg->t_obj;
    int tid = threadarg->tid;
#ifdef ENABLE_PARSEC_HOOKS
    bool set_range = false;
    int cpu_base = 0, cpu_num = 0;
    char *str_num = getenv(__PARSEC_CPU_NUM);
    char *str_base = getenv(__PARSEC_CPU_BASE);
    if ((str_num != NULL)&& (str_base != NULL)) {
      cpu_num = atoi(str_num);
      cpu_base = atoi(str_base);
      set_range = true;
    }
    //set affinity
    if(set_range) {
      cpu_set_t mask;
      CPU_ZERO(&mask);
      int core_id = cpu_base + (tid % cpu_num);
      CPU_SET(core_id, &mask);
      if (!sched_setaffinity(0, sizeof(mask), &mask)) {
        fprintf(stderr, "thread %d bound to core %d\n", tid, core_id);
	__parsec_binding_done((unsigned int)core_id);
      } else {
        fprintf(stderr, " Error: failure in setting affinity for thread %d\n", tid);
      }
    }
#endif // ENABLE_PARSEC_HOOKS
    tobj->Run();

    return NULL;
  }
}
#else //default: winthreads
extern "C" {
  unsigned __stdcall thread_entry(void *arg) {
    Runnable *tobj = static_cast<Runnable *>(arg);
    tobj->Run();
	return NULL;
  }
}
#endif //HAVE_LIBPTHREAD


//Constructor, expects a threadable object as argument
Thread::Thread(Runnable &_tobj) throw(ThreadCreationException) : tobj(_tobj) {
//Abhi===
  thread_arg.t_obj = &tobj;
  //Thread::M->Lock();
  thread_arg.tid = Thread::thread_count++;
  //Thread::M->Unlock();
  //=======
#if defined(HAVE_LIBPTHREAD)
  //Abhi===
  //if(pthread_create(&t, NULL, &thread_entry, (void *)&tobj)) {
  if(pthread_create(&t, NULL, &thread_entry, (void *)&thread_arg)) {
    //=====
    ThreadCreationException e;
    throw e;
  }

#else //default: winthreads
  t = (void *)_beginthreadex(NULL, 0, &thread_entry, (void *)&tobj, 0, &t_id);
  if(!t) {
    ThreadCreationException e;
    throw e;
  }
#endif //HAVE_LIBPTHREAD
}

//Wait until Thread object has finished
void Thread::Join() {
  Stoppable *_tobj;
  //Abhi for some reason the try catch is failing to catch the exception
  //bool isStoppable = true;
  bool isStoppable = false;
  //call Stop() function if implemented
  //   try {
  //     _tobj = &dynamic_cast<Stoppable &>(tobj);
  //   } catch(std::bad_cast e) {
  //     isStoppable = false;
  //   }
  //====
  if(isStoppable) {
    _tobj->Stop();
  }

#if defined(HAVE_LIBPTHREAD)
  pthread_join(t, NULL);
#else //default: winthreads
  WaitForSingleObject(t, INFINITE);
  CloseHandle(t);
#endif //HAVE_LIBPTHREAD
}

} //namespace threads
