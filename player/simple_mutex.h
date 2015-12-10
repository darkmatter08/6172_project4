// Copyright (c) 2015 MIT License by 6.172 Staff
// A simple implementation of a spin lock using compare and swap.
#include <pthread.h>

#ifndef SIMPLE_MUTEX_H
#define SIMPLE_MUTEX_H


typedef pthread_spinlock_t simple_mutex_t;

void init_simple_mutex(simple_mutex_t* mutex) {
  pthread_spin_init(mutex, PTHREAD_PROCESS_PRIVATE);
}

void simple_acquire(simple_mutex_t* mutex) {
  pthread_spin_lock(mutex);
}

void simple_release(simple_mutex_t* mutex) {
  pthread_spin_unlock(mutex);
}

#endif  // SIMPLE_MUTEX_H
