// (C) Copyright Christian Bienia 2007
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0.
//
//  file : WorkerGroup.h
//  author : Christian Bienia - cbienia@cs.princeton.edu
//  description : A class which can manage a group of worker threads



#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include <climits>

#include "WorkerGroup.h"

#include "Thread.h"
#include "ThreadGroup.h"
#include "Mutex.h"
#include "Condition.h"
#include "Barrier.h"
#include "TicketDispenser.h"
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

namespace threads{

//constructor
WorkerGroup::WorkerGroup(int nThreads) : cmd(THREADS_IDLE), workAvailable(workDispatch) {
  if(nThreads < 1) {
    WorkerGroupException e;
    throw e;
  }
  
  workDoneBarrier = new threads::Barrier(nThreads);
  poolReadyBarrier = new threads::Barrier(nThreads + 1);
  tCreatedBarrier = new threads::Barrier(nThreads + 1); //Abhi
  ThreadGroup::CreateThreads(nThreads, *this);
  tCreatedBarrier->Wait(); //Abhi == wait till all threads are created
}

//destructor
WorkerGroup::~WorkerGroup() {
  delete workDoneBarrier;
  delete poolReadyBarrier;
  delete tCreatedBarrier; //Abhi
}

//Add a new command
void WorkerGroup::RegisterCmd(int _cmd, Threadable &obj) {
  if(_cmd < 0) {
    WorkerGroupCommandRangeException e;
    throw e;
  }
  if(_cmd > USHRT_MAX) {
    WorkerGroupCommandRangeException e;
    throw e;
  }
  RegisterCmd((thread_cmd_t)_cmd, obj);
}

//Add a new command
void WorkerGroup::RegisterCmd(thread_cmd_t _cmd, Threadable &obj) {
  if(_cmd >= cmds.size()) {
    cmds.resize(_cmd + 1, NULL);
  }
  cmds[_cmd] = &obj;
}

//Send an internal command to all worker threads
void WorkerGroup::SendInternalCmd(thread_internal_cmd_t _cmd) {
  workDispatch.Lock();
  //send command
  cmd = _cmd;
  workAvailable.NotifyAll();
  workDispatch.Unlock();

  //wait until all work is done and pool is ready
  poolReadyBarrier->Wait();
}

//Send a command to all worker threads
void WorkerGroup::SendCmd(thread_cmd_t _cmd) {
  if(_cmd >= cmds.size()) {
    WorkerGroupCommandException e;
    throw e;
  }
  if(cmds[_cmd] == NULL) {
    WorkerGroupCommandException e;
    throw e;
  }

  SendInternalCmd((thread_internal_cmd_t)_cmd);
}

//Receive command
WorkerGroup::thread_internal_cmd_t WorkerGroup::RecvCmd() {
  thread_internal_cmd_t _cmd;

  workDispatch.Lock();
  //wait until work has been assigned
  while(cmd == THREADS_IDLE) workAvailable.Wait();
  _cmd = cmd;
  workDispatch.Unlock();

  return _cmd;
}

//Acknowledge completion of command
void WorkerGroup::AckCmd() {
  bool master;

  master = workDoneBarrier->Wait();
  if(master) {
    workDispatch.Lock();
    cmd = THREADS_IDLE;
    workDispatch.Unlock();
  }
  poolReadyBarrier->Wait();
}

//thread entry function	
void WorkerGroup::Run() {
  bool doExit = false;
  static thread_rank_t counter = 0;
  thread_rank_t rank;
  thread_internal_cmd_t cmd;
  //determine rank of this thread
  workDispatch.Lock();
  rank = counter;
  counter++;
  workDispatch.Unlock();
  //Abhi ==bind the threads to a core
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
    int core_id = cpu_base + (rank % cpu_num);
    CPU_SET(core_id, &mask);
    if (!sched_setaffinity(0, sizeof(mask), &mask)) {
      fprintf(stderr, "thread %d bound to core %d\n", rank, core_id);
      __parsec_binding_done((unsigned int)core_id);
    } else {
      fprintf(stderr, " Error: failure in setting affinity for thread %d\n", rank);
    }
  }
#endif // ENABLE_PARSEC_HOOKS
  // Abhi: call the barrier after thread is created and bound
  tCreatedBarrier->Wait();
  //worker thread main loop
  while(!doExit) {
    //wait until work has been assigned
    cmd = RecvCmd();

    switch(cmd) {
      case THREADS_IDLE:
      {
        WorkerGroupException e;
        throw e;
        break;
      }
      case THREADS_SHUTDOWN:
        doExit = true;
        break;
      default:
        //do work
        cmds[cmd]->Exec((thread_cmd_t)cmd, rank);
        break;
    }

    //confirm completion of command
    AckCmd();
  }
}

//Terminate all threads in the group
void WorkerGroup::JoinAll() {
  SendInternalCmd(THREADS_SHUTDOWN);
  ThreadGroup::JoinAll();
}

//Number of workers in group
int WorkerGroup::Size() const {
  return ThreadGroup::Size();
}

} //namespace threads
