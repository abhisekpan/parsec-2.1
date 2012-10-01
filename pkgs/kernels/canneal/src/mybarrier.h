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
  volatile bool m_flag;
  volatile int m_counter;
  volatile bool *m_local_sense;
  pthread_spinlock_t m_lock;
};

inline MyBarrier::MyBarrier(int participants)
: m_participants(participants),
  m_flag(false),
  m_counter(participants) {
  assert (participants > 0);
  m_local_sense = new (std::nothrow) bool[m_participants];
  assert (m_local_sense != NULL);
  for (int i = 0; i < m_participants; ++i) {
    m_local_sense[i] = false;
  }
  int pshared = 0;
  int ret = pthread_spin_init(&m_lock, pshared); 
  assert (ret == 0);  
}

inline MyBarrier::~MyBarrier( ) {
  delete [] m_local_sense;
  pthread_spin_destroy(&m_lock);
}

inline int MyBarrier::Wait(int tid) {
  assert (tid < m_participants);
  m_local_sense[tid] = !m_local_sense[tid];
  int lock_ret = pthread_spin_lock(&m_lock);
  assert (lock_ret == 0);
  --m_counter;
  if (m_counter == 0) {
    int unlock_ret = pthread_spin_unlock(&m_lock);
    assert (unlock_ret == 0);
    m_counter = m_participants;
    m_flag = m_local_sense[tid];
    return PTHREAD_BARRIER_SERIAL_THREAD;
  } else {
    int unlock_ret = pthread_spin_unlock(&m_lock);
    assert (unlock_ret == 0);
    __parsec_local_end(); // keep the busy loop out of data collection or simulation
    while (m_flag != m_local_sense[tid]) {
      asm volatile("rep; nop" ::: "memory");
      asm volatile("rep; nop" ::: "memory");
    }
    __parsec_local_start();
    return 0;
  }
}

#endif //MYBARRIER_H
