// A centralized busy-wait barrier with sense reversal

#ifndef MYBARRIER_H_
#define MYBARRIER_H_

#include <cassert>
#include <new>
#include <pthread.h>
#include <iostream>

#include "hooks.h"
class MyBarrier {
 public:
  MyBarrier(int participants);
  ~MyBarrier();
  int Wait(int tid);
  
 private:
  int m_participants;
  volatile bool m_global_sense;
  volatile int m_counter;
  bool *m_local_sense;
  int *m_local_count;
  pthread_spinlock_t m_lock;
};

inline MyBarrier::MyBarrier(int participants)
: m_participants(participants),
  m_global_sense(false),
  m_counter(participants) {
  assert (participants > 0);
  m_local_sense = new (std::nothrow) bool[m_participants];
  assert (m_local_sense != NULL);
  m_local_count = new (std::nothrow) int[m_participants];
  assert (m_local_count != NULL);
  for (int i = 0; i < m_participants; ++i) {
    m_local_sense[i] = false;
    m_local_count[i] = 0;
  }
  int pshared = PTHREAD_PROCESS_PRIVATE;
  int ret = pthread_spin_init(&m_lock, pshared); 
  assert (ret == 0);  
}

inline MyBarrier::~MyBarrier( ) {
  delete [] m_local_sense;
  delete [] m_local_count;
  pthread_spin_destroy(&m_lock);
  //check that barrier is no longer active - extra checking
  assert (m_counter == m_participants);
}

inline int MyBarrier::Wait(int tid) {
  assert (tid < m_participants);
  m_local_sense[tid] = !m_local_sense[tid];
  int lock_ret = pthread_spin_lock(&m_lock);
  assert (lock_ret == 0);
  m_local_count[tid] = --m_counter; 
  if (m_local_count[tid] == 0) {
    m_counter = m_participants;
    m_global_sense = m_local_sense[tid];
    int unlock_ret = pthread_spin_unlock(&m_lock);
    assert (unlock_ret == 0);
    return PTHREAD_BARRIER_SERIAL_THREAD;
  } else {
    int unlock_ret = pthread_spin_unlock(&m_lock);
    assert (unlock_ret == 0);
    __parsec_local_end(); // keep the busy loop out of data collection or simulation
    while (m_global_sense != m_local_sense[tid]) {
      asm volatile("rep; nop" ::: "memory");
      asm volatile("rep; nop" ::: "memory");
    }
    __parsec_local_start();
    return 0;
  }
}

#endif //MYBARRIER_H
