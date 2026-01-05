#pragma once

/*
 * Copyright 2025 Theo Lincke
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 *   Interface wrappers of threading related objects
 */

#include <numstore/core/bytes.h>
#include <numstore/core/error.h>
#include <numstore/core/system.h>

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>

////////////////////////////////////////////////////////////
// Mutex

typedef struct
{
  pthread_mutex_t m;
} i_mutex;

err_t i_mutex_create (i_mutex *m, error *e);
void i_mutex_free (i_mutex *m);
void i_mutex_lock (i_mutex *m);
bool i_mutex_try_lock (i_mutex *m);
void i_mutex_unlock (i_mutex *m);

////////////////////////////////////////////////////////////
// Spinlock

typedef struct
{
  pthread_spinlock_t lock;
} i_spinlock;

err_t i_spinlock_create (i_spinlock *m, error *e);
void i_spinlock_free (i_spinlock *m);
void i_spinlock_lock (i_spinlock *m);
void i_spinlock_unlock (i_spinlock *m);

////////////////////////////////////////////////////////////
// RW Lock

typedef struct
{
  pthread_rwlock_t lock;
} i_rwlock;

err_t i_rwlock_create (i_rwlock *rw, error *e);
void i_rwlock_free (i_rwlock *rw);
void i_rwlock_rdlock (i_rwlock *rw);
void i_rwlock_wrlock (i_rwlock *rw);
void i_rwlock_unlock (i_rwlock *rw);
void i_rwlock_lock (i_rwlock *m);
void i_rwlock_lock (i_rwlock *m);
bool i_rwlock_try_lock (i_rwlock *m);
bool i_rwlock_try_lock (i_rwlock *m);
void i_rwlock_unlock (i_rwlock *m);
void i_rwlock_unlock (i_rwlock *m);

////////////////////////////////////////////////////////////
// Thread

typedef struct
{
  pthread_t thread;
} i_thread;

err_t i_thread_create (i_thread *t, void *(*start_routine) (void *), void *arg, error *e);
err_t i_thread_join (i_thread *t, error *e);
void i_thread_cancel (i_thread *t);
u64 get_available_threads (void);

////////////////////////////////////////////////////////////
// Condition Variable

typedef struct
{
  pthread_cond_t cond;
} i_cond;

err_t i_cond_create (i_cond *c, error *e);
void i_cond_free (i_cond *c);
void i_cond_wait (i_cond *c, i_mutex *m);
void i_cond_signal (i_cond *c);
void i_cond_broadcast (i_cond *c);
