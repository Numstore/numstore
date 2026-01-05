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
 *   Cross-platform OS abstraction layer for file I/O, timers, memory allocation,
 *   and synchronization primitives (mutexes, spinlocks, rwlocks, threads, condvars).
 */

#include <numstore/core/bytes.h>
#include <numstore/core/error.h>
#include <numstore/core/system.h>

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>

////////////////////////////////////////////////////////////
// File Handle

typedef struct i_file i_file;

#if PLATFORM_WINDOWS
struct i_file
{
  HANDLE handle;
};
#else
struct i_file
{
  int fd;
};
#endif

////////////////////////////////////////////////////////////
// Open / Close
err_t i_open_rw (i_file *dest, const char *fname, error *e);
err_t i_open_r (i_file *dest, const char *fname, error *e);
err_t i_open_w (i_file *dest, const char *fname, error *e);
err_t i_close (i_file *fp, error *e);
err_t i_eof (i_file *fp, error *e);
err_t i_fsync (i_file *fp, error *e);

////////////////////////////////////////////////////////////
// Positional Read / Write
i64 i_pread_some (i_file *fp, void *dest, u64 n, u64 offset, error *e);
i64 i_pread_all (i_file *fp, void *dest, u64 n, u64 offset, error *e);
err_t i_pread_all_expect (i_file *fp, void *dest, u64 n, u64 offset, error *e);
i64 i_pwrite_some (i_file *fp, const void *src, u64 n, u64 offset, error *e);
err_t i_pwrite_all (i_file *fp, const void *src, u64 n, u64 offset, error *e);

////////////////////////////////////////////////////////////
// IO Vec
i64 i_writev_some (i_file *fp, const struct bytes *arrs, int iovcnt, error *e);
err_t i_writev_all (i_file *fp, struct bytes *arrs, int iovcnt, error *e);
i64 i_readv_some (i_file *fp, const struct bytes *arrs, int iovcnt, error *e);
i64 i_readv_all (i_file *fp, struct bytes *arrs, int iovcnt, error *e);

////////////////////////////////////////////////////////////
// File Stream

typedef struct i_stream i_stream;

struct i_stream
{
  FILE *fp;
};

////////////////////////////////////////////////////////////
// Open / Close

err_t i_stream_open_rw (i_stream *dest, const char *fname, error *e);
err_t i_stream_open_r (i_stream *dest, const char *fname, error *e);
err_t i_stream_open_w (i_stream *dest, const char *fname, error *e);
err_t i_stream_close (i_stream *fp, error *e);
err_t i_stream_eof (i_stream *fp, error *e);
err_t i_stream_fsync (i_stream *fp, error *e);
i64 i_stream_read_some (i_stream *fp, void *dest, u64 nbytes, error *e);
i64 i_stream_read_all (i_stream *fp, void *dest, u64 nbytes, error *e);
i64 i_stream_read_all_expect (i_stream *fp, void *dest, u64 nbytes, error *e);
i64 i_stream_write_some (i_stream *fp, const void *src, u64 nbytes, error *e);
err_t i_stream_write_all (i_stream *fp, const void *src, u64 nbytes, error *e);

////////////////////////////////////////////////////////////
// Runtime
void i_print_stack_trace (void);

////////////////////////////////////////////////////////////
// Timer Handle
typedef struct i_timer i_timer;

#if PLATFORM_WINDOWS
struct i_timer
{
  LARGE_INTEGER frequency;
  LARGE_INTEGER start;
};
#elif PLATFORM_MAC || PLATFORM_IOS
struct i_timer
{
  u64 start;
  u64 numer;
  u64 denom;
};
#elif PLATFORM_EMSCRIPTEN
struct i_timer
{
  f64 start;
};
#else // POSIX (Linux, Android, BSD)
struct i_timer
{
  struct timespec start;
};
#endif

err_t i_timer_create (i_timer *timer, error *e);
void i_timer_free (i_timer *timer);
u64 i_timer_now_ns (i_timer *timer);
u64 i_timer_now_us (i_timer *timer);
u64 i_timer_now_ms (i_timer *timer);
f64 i_timer_now_s (i_timer *timer);

// Legacy API (deprecated - use i_timer instead)
void i_get_monotonic_time (struct timespec *ts);

////////////////////////////////////////////////////////////
// Stream Read / Write
i64 i_read_some (i_file *fp, void *dest, u64 nbytes, error *e);
i64 i_read_all (i_file *fp, void *dest, u64 nbytes, error *e);
i64 i_read_all_expect (i_file *fp, void *dest, u64 nbytes, error *e);
i64 i_write_some (i_file *fp, const void *src, u64 nbytes, error *e);
err_t i_write_all (i_file *fp, const void *src, u64 nbytes, error *e);

////////////////////////////////////////////////////////////
// Others
err_t i_truncate (i_file *fp, u64 bytes, error *e);
i64 i_file_size (i_file *fp, error *e);
err_t i_remove_quiet (const char *fname, error *e);
err_t i_mkstemp (i_file *dest, char *tmpl, error *e);
err_t i_unlink (const char *name, error *e);

typedef enum
{
  I_SEEK_END,
  I_SEEK_CUR,
  I_SEEK_SET,
} seek_t;

i64 i_seek (i_file *fp, u64 offset, seek_t whence, error *e);

////////////////////////////////////////////////////////////
// Wrappers
err_t i_access_rw (const char *fname, error *e);
bool i_exists_rw (const char *fname);
err_t i_touch (const char *fname, error *e);

////////////////////////////////////////////////////////////
// Memory
void *i_malloc (u32 nelem, u32 size, error *e);
void *i_calloc (u32 nelem, u32 size, error *e);
void *i_realloc_right (void *ptr, u32 old_nelem, u32 nelem, u32 size, error *e);
void *i_realloc_left (void *ptr, u32 old_nelem, u32 nelem, u32 size, error *e);
void *i_crealloc_right (void *ptr, u32 old_nelem, u32 nelem, u32 size, error *e);
void *i_crealloc_left (void *ptr, u32 old_nelem, u32 nelem, u32 size, error *e);
void i_free (void *ptr);
#define i_cfree(ptr)    \
  do                    \
    {                   \
      if (ptr)          \
        {               \
          i_free (ptr); \
        }               \
    }                   \
  while (0)

////////////////////////////////////////////////////////////
// Mutex
#if PLATFORM_WINDOWS
typedef struct
{
  CRITICAL_SECTION cs;
} i_mutex;
#else
typedef struct
{
  pthread_mutex_t m;
} i_mutex;
#endif

err_t i_mutex_create (i_mutex *m, error *e);
void i_mutex_free (i_mutex *m);
void i_mutex_lock (i_mutex *m);
bool i_mutex_try_lock (i_mutex *m);
void i_mutex_unlock (i_mutex *m);

////////////////////////////////////////////////////////////
// Spinlock
#if PLATFORM_WINDOWS
typedef struct
{
  CRITICAL_SECTION lock;
} i_spinlock;
#else
typedef struct
{
  pthread_spinlock_t lock;
} i_spinlock;
#endif

err_t i_spinlock_create (i_spinlock *m, error *e);
void i_spinlock_free (i_spinlock *m);
void i_spinlock_lock (i_spinlock *m);
void i_spinlock_unlock (i_spinlock *m);

////////////////////////////////////////////////////////////
// RW Lock
#if PLATFORM_WINDOWS
typedef struct
{
  SRWLOCK lock;
} i_rwlock;
#else
typedef struct
{
  pthread_rwlock_t lock;
} i_rwlock;
#endif

err_t i_rwlock_create (i_rwlock *rw, error *e);
void i_rwlock_free (i_rwlock *rw);
void i_rwlock_rdlock (i_rwlock *rw);
void i_rwlock_wrlock (i_rwlock *rw);
void i_rwlock_unlock (i_rwlock *rw);

////////////////////////////////////////////////////////////
// Thread
#if PLATFORM_WINDOWS
typedef struct
{
  HANDLE handle;
} i_thread;
#else
typedef struct
{
  pthread_t thread;
} i_thread;
#endif

err_t i_thread_create (i_thread *t, void *(*start_routine) (void *), void *arg, error *e);
err_t i_thread_join (i_thread *t, error *e);

////////////////////////////////////////////////////////////
// Condition Variable
#if PLATFORM_WINDOWS
typedef struct
{
  CONDITION_VARIABLE cond;
} i_cond;
#else
typedef struct
{
  pthread_cond_t cond;
} i_cond;
#endif

err_t i_cond_create (i_cond *c, error *e);
void i_cond_free (i_cond *c);
void i_cond_wait (i_cond *c, i_mutex *m);
void i_cond_signal (i_cond *c);
void i_cond_broadcast (i_cond *c);

// Additional RW Lock functions
void i_rwlock_s_lock (i_rwlock *m);
void i_rwlock_x_lock (i_rwlock *m);

bool i_rwlock_s_try_lock (i_rwlock *m);
bool i_rwlock_x_try_lock (i_rwlock *m);

void i_rwlock_s_unlock (i_rwlock *m);
void i_rwlock_x_unlock (i_rwlock *m);

// Additional Thread functions
void i_thread_cancel (i_thread *t);
u64 get_available_threads (void);
