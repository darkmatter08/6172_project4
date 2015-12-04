// Copyright (c) 2015 MIT License by 6.172 Staff
#include <xmmintrin.h>
// A simple implementation of a spin lock using compare and swap.
#ifndef SIMPLE_MUTEX_H
#define SIMPLE_MUTEX_H


typedef int simple_mutex_t;

void init_simple_mutex(simple_mutex_t* mutex) {
  *mutex = 0;
}

void simple_acquire(simple_mutex_t* mutex) {
  while (!__sync_bool_compare_and_swap(mutex, 0, 1)) {
    _mm_pause();
    //usleep(1);
    //continue;  // Did not acquire lock yet.
  }
}

void simple_release(simple_mutex_t* mutex) {
  /*if (!__sync_bool_compare_and_swap(mutex, 1, 0)) {
    printf("ERROR!\n");
  }*/
  *mutex = 0;
}

#endif  // SIMPLE_MUTEX_H
