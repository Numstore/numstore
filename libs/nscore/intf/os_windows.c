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
 *   Implements os.h for Windows platform-specific OS operations.
 */

// core
#include <numstore/core/assert.h>
#include <numstore/core/bounds.h>
#include <numstore/core/error.h>
#include <numstore/core/filenames.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/intf/types.h>
#include <numstore/test/testing.h>

#include <io.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <windows.h>

// os
// system
#undef bool

static inline bool
handle_is_valid (HANDLE h)
{
  return h != INVALID_HANDLE_VALUE && h != NULL;
}

DEFINE_DBG_ASSERT (
    i_file, i_file, fp,
    {
      ASSERT (fp);
      ASSERT (handle_is_valid (fp->handle));
    })

////////////////////////////////////////////////////////////
// OPEN / CLOSE
err_t
i_open_rw (i_file *dest, const char *fname, error *e)
{
  HANDLE h = CreateFileA (
      fname,
      GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL,
      OPEN_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,
      NULL);

  if (!handle_is_valid (h))
    {
      return error_causef (e, ERR_IO, "open_rw %s: Error %lu", fname, GetLastError ());
    }
  dest->handle = h;

  DBG_ASSERT (i_file, dest);

  return SUCCESS;
}

err_t
i_open_r (i_file *dest, const char *fname, error *e)
{
  HANDLE h = CreateFileA (
      fname,
      GENERIC_READ,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL,
      OPEN_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,
      NULL);

  if (!handle_is_valid (h))
    {
      return error_causef (e, ERR_IO, "open_r %s: Error %lu", fname, GetLastError ());
    }
  dest->handle = h;

  DBG_ASSERT (i_file, dest);

  return SUCCESS;
}

err_t
i_open_w (i_file *dest, const char *fname, error *e)
{
  HANDLE h = CreateFileA (
      fname,
      GENERIC_WRITE,
      FILE_SHARE_READ,
      NULL,
      OPEN_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,
      NULL);

  if (!handle_is_valid (h))
    {
      return error_causef (e, ERR_IO, "open_w %s: Error %lu", fname, GetLastError ());
    }
  dest->handle = h;

  DBG_ASSERT (i_file, dest);

  return SUCCESS;
}

err_t
i_close (i_file *fp, error *e)
{
  DBG_ASSERT (i_file, fp);
  if (!CloseHandle (fp->handle))
    {
      return error_causef (e, ERR_IO, "close: Error %lu", GetLastError ());
    }
  return SUCCESS;
}

err_t
i_fsync (i_file *fp, error *e)
{
  DBG_ASSERT (i_file, fp);
  if (!FlushFileBuffers (fp->handle))
    {
      return error_causef (e, ERR_IO, "fsync: Error %lu", GetLastError ());
    }
  return SUCCESS;
}

//////////////// Positional Read / Write */

i64
i_pread_some (i_file *fp, void *dest, u64 n, u64 offset, error *e)
{
  DBG_ASSERT (i_file, fp);
  ASSERT (dest);
  ASSERT (n > 0);

  OVERLAPPED overlapped = { 0 };
  overlapped.Offset = (DWORD) (offset & 0xFFFFFFFF);
  overlapped.OffsetHigh = (DWORD) (offset >> 32);

  DWORD nread;
  if (!ReadFile (fp->handle, dest, (DWORD)n, &nread, &overlapped))
    {
      DWORD err = GetLastError ();
      if (err != ERROR_HANDLE_EOF)
        {
          return error_causef (e, ERR_IO, "pread: Error %lu", err);
        }
    }

  return (i64)nread;
}

i64
i_pread_all (i_file *fp, void *dest, u64 n, u64 offset, error *e)
{
  DBG_ASSERT (i_file, fp);
  ASSERT (dest);
  ASSERT (n > 0);

  u8 *_dest = (u8 *)dest;
  u64 nread = 0;

  while (nread < n)
    {
      OVERLAPPED overlapped = { 0 };
      u64 current_offset = offset + nread;
      overlapped.Offset = (DWORD) (current_offset & 0xFFFFFFFF);
      overlapped.OffsetHigh = (DWORD) (current_offset >> 32);

      DWORD _nread;
      if (!ReadFile (fp->handle, _dest + nread, (DWORD) (n - nread), &_nread, &overlapped))
        {
          DWORD err = GetLastError ();
          if (err == ERROR_HANDLE_EOF)
            {
              break;
            }
          return error_causef (e, ERR_IO, "pread_all: Error %lu", err);
        }

      if (_nread == 0)
        {
          break;
        }

      nread += _nread;
    }

  return (i64)nread;
}

err_t
i_pread_all_expect (i_file *fp, void *dest, u64 n, u64 offset, error *e)
{
  i64 nread = i_pread_all (fp, dest, n, offset, e);
  if (nread < 0)
    {
      return e->cause_code;
    }

  if ((u64)nread != n)
    {
      return error_causef (e, ERR_IO, "Expected %" PRu64 " bytes but read %" PRi64, n, nread);
    }

  return SUCCESS;
}

i64
i_pwrite_some (i_file *fp, const void *src, u64 n, u64 offset, error *e)
{
  DBG_ASSERT (i_file, fp);
  ASSERT (src);
  ASSERT (n > 0);

  OVERLAPPED overlapped = { 0 };
  overlapped.Offset = (DWORD) (offset & 0xFFFFFFFF);
  overlapped.OffsetHigh = (DWORD) (offset >> 32);

  DWORD nwritten;
  if (!WriteFile (fp->handle, src, (DWORD)n, &nwritten, &overlapped))
    {
      return error_causef (e, ERR_IO, "pwrite: Error %lu", GetLastError ());
    }

  return (i64)nwritten;
}

err_t
i_pwrite_all (i_file *fp, const void *src, u64 n, u64 offset, error *e)
{
  DBG_ASSERT (i_file, fp);
  ASSERT (src);
  ASSERT (n > 0);

  const u8 *_src = (const u8 *)src;
  u64 nwritten = 0;

  while (nwritten < n)
    {
      OVERLAPPED overlapped = { 0 };
      u64 current_offset = offset + nwritten;
      overlapped.Offset = (DWORD) (current_offset & 0xFFFFFFFF);
      overlapped.OffsetHigh = (DWORD) (current_offset >> 32);

      DWORD _nwritten;
      if (!WriteFile (fp->handle, _src + nwritten, (DWORD) (n - nwritten), &_nwritten, &overlapped))
        {
          return error_causef (e, ERR_IO, "pwrite_all: Error %lu", GetLastError ());
        }

      nwritten += _nwritten;
    }

  return SUCCESS;
}

////////////////////////////////////////////////////////////
// Runtime
void
i_print_stack_trace (void)
{
  // Windows stack trace implementation is more complex
  // For now, just print a message
  i_log_error ("Stack trace not implemented on Windows\n");
}

////////////////////////////////////////////////////////////
// Stream Read / Write
i64
i_read_some (i_file *fp, void *dest, u64 nbytes, error *e)
{
  DBG_ASSERT (i_file, fp);
  ASSERT (dest);

  DWORD nread;
  if (!ReadFile (fp->handle, dest, (DWORD)nbytes, &nread, NULL))
    {
      DWORD err = GetLastError ();
      if (err != ERROR_HANDLE_EOF)
        {
          return error_causef (e, ERR_IO, "read: Error %lu", err);
        }
    }

  return (i64)nread;
}

i64
i_read_all (i_file *fp, void *dest, u64 nbytes, error *e)
{
  u8 *_dest = (u8 *)dest;
  u64 nread = 0;

  while (nread < nbytes)
    {
      i64 ret = i_read_some (fp, _dest + nread, nbytes - nread, e);
      if (ret < 0)
        {
          return ret;
        }
      if (ret == 0)
        {
          break;
        }
      nread += ret;
    }

  return (i64)nread;
}

i64
i_read_all_expect (i_file *fp, void *dest, u64 nbytes, error *e)
{
  i64 nread = i_read_all (fp, dest, nbytes, e);
  if (nread < 0)
    {
      return nread;
    }

  if ((u64)nread != nbytes)
    {
      return error_causef (e, ERR_IO, "Expected %" PRu64 " bytes but read %" PRi64, nbytes, nread);
    }

  return nread;
}

i64
i_write_some (i_file *fp, const void *src, u64 nbytes, error *e)
{
  DBG_ASSERT (i_file, fp);
  ASSERT (src);

  DWORD nwritten;
  if (!WriteFile (fp->handle, src, (DWORD)nbytes, &nwritten, NULL))
    {
      return error_causef (e, ERR_IO, "write: Error %lu", GetLastError ());
    }

  return (i64)nwritten;
}

err_t
i_write_all (i_file *fp, const void *src, u64 nbytes, error *e)
{
  const u8 *_src = (const u8 *)src;
  u64 nwritten = 0;

  while (nwritten < nbytes)
    {
      i64 ret = i_write_some (fp, _src + nwritten, nbytes - nwritten, e);
      if (ret < 0)
        {
          return e->cause_code;
        }
      nwritten += ret;
    }

  return SUCCESS;
}

////////////////////////////////////////////////////////////
// Others
err_t
i_truncate (i_file *fp, u64 bytes, error *e)
{
  DBG_ASSERT (i_file, fp);

  LARGE_INTEGER li;
  li.QuadPart = bytes;

  if (!SetFilePointerEx (fp->handle, li, NULL, FILE_BEGIN))
    {
      return error_causef (e, ERR_IO, "truncate seek: Error %lu", GetLastError ());
    }

  if (!SetEndOfFile (fp->handle))
    {
      return error_causef (e, ERR_IO, "truncate: Error %lu", GetLastError ());
    }

  return SUCCESS;
}

i64
i_file_size (i_file *fp, error *e)
{
  DBG_ASSERT (i_file, fp);

  LARGE_INTEGER size;
  if (!GetFileSizeEx (fp->handle, &size))
    {
      error_causef (e, ERR_IO, "file_size: Error %lu", GetLastError ());
      return -1;
    }

  return (i64)size.QuadPart;
}

err_t
i_remove_quiet (const char *fname, error *e)
{
  if (!DeleteFileA (fname))
    {
      DWORD err = GetLastError ();
      if (err != ERROR_FILE_NOT_FOUND)
        {
          return error_causef (e, ERR_IO, "remove %s: Error %lu", fname, err);
        }
    }
  return SUCCESS;
}

err_t
i_mkstemp (i_file *dest, char *tmpl, error *e)
{
  // Simple implementation using _mktemp_s and CreateFile
  errno_t err_no = _mktemp_s (tmpl, strlen (tmpl) + 1);
  if (err_no != 0)
    {
      return error_causef (e, ERR_IO, "mkstemp: _mktemp_s failed");
    }

  return i_open_rw (dest, tmpl, e);
}

err_t
i_unlink (const char *name, error *e)
{
  if (!DeleteFileA (name))
    {
      return error_causef (e, ERR_IO, "unlink %s: Error %lu", name, GetLastError ());
    }
  return SUCCESS;
}

i64
i_seek (i_file *fp, u64 offset, seek_t whence, error *e)
{
  DBG_ASSERT (i_file, fp);

  DWORD move_method;
  switch (whence)
    {
    case I_SEEK_SET:
      move_method = FILE_BEGIN;
      break;
    case I_SEEK_CUR:
      move_method = FILE_CURRENT;
      break;
    case I_SEEK_END:
      move_method = FILE_END;
      break;
    default:
      return error_causef (e, ERR_IO, "seek: invalid whence");
    }

  LARGE_INTEGER li;
  li.QuadPart = offset;
  LARGE_INTEGER new_pos;

  if (!SetFilePointerEx (fp->handle, li, &new_pos, move_method))
    {
      return error_causef (e, ERR_IO, "seek: Error %lu", GetLastError ());
    }

  return (i64)new_pos.QuadPart;
}

err_t
i_eof (i_file *fp, error *e)
{
  i64 size = i_file_size (fp, e);
  if (size < 0)
    {
      return e->cause_code;
    }

  i64 pos = i_seek (fp, 0, I_SEEK_CUR, e);
  if (pos < 0)
    {
      return e->cause_code;
    }

  return pos >= size ? ERR_IO : SUCCESS;
}

////////////////////////////////////////////////////////////
// Wrappers
err_t
i_access_rw (const char *fname, error *e)
{
  if (_access (fname, 0) == -1)
    {
      return error_causef (e, ERR_IO, "access %s: file not found", fname);
    }
  return SUCCESS;
}

bool
i_exists_rw (const char *fname)
{
  return _access (fname, 0) != -1;
}

err_t
i_touch (const char *fname, error *e)
{
  i_file fp;
  err_t ret = i_open_rw (&fp, fname, e);
  if (ret)
    {
      return ret;
    }
  return i_close (&fp, e);
}

////////////////////////////////////////////////////////////
// Memory
void *
i_malloc (u32 nelem, u32 size, error *e)
{
  void *ptr = malloc ((size_t)nelem * size);
  if (!ptr)
    {
      error_causef (e, ERR_NOMEM, "malloc failed");
    }
  return ptr;
}

void *
i_calloc (u32 nelem, u32 size, error *e)
{
  void *ptr = calloc (nelem, size);
  if (!ptr)
    {
      error_causef (e, ERR_NOMEM, "calloc failed");
    }
  return ptr;
}

void *
i_realloc_right (void *ptr, u32 old_nelem, u32 nelem, u32 size, error *e)
{
  void *new_ptr = realloc (ptr, (size_t)nelem * size);
  if (!new_ptr && nelem > 0)
    {
      error_causef (e, ERR_NOMEM, "realloc failed");
      return NULL;
    }
  return new_ptr;
}

void *
i_realloc_left (void *ptr, u32 old_nelem, u32 nelem, u32 size, error *e)
{
  // For Windows, we just do a regular realloc
  return i_realloc_right (ptr, old_nelem, nelem, size, e);
}

void *
i_crealloc_right (void *ptr, u32 old_nelem, u32 nelem, u32 size, error *e)
{
  void *new_ptr = i_realloc_right (ptr, old_nelem, nelem, size, e);
  if (new_ptr && nelem > old_nelem)
    {
      memset ((u8 *)new_ptr + old_nelem * size, 0, (nelem - old_nelem) * size);
    }
  return new_ptr;
}

void *
i_crealloc_left (void *ptr, u32 old_nelem, u32 nelem, u32 size, error *e)
{
  return i_crealloc_right (ptr, old_nelem, nelem, size, e);
}

void
i_free (void *ptr)
{
  free (ptr);
}

////////////////////////////////////////////////////////////
// Mutex
err_t
i_mutex_create (i_mutex *m, error *e)
{
  InitializeCriticalSection (&m->cs);
  return SUCCESS;
}

void
i_mutex_free (i_mutex *m)
{
  DeleteCriticalSection (&m->cs);
}

void
i_mutex_lock (i_mutex *m)
{
  EnterCriticalSection (&m->cs);
}

void
i_mutex_unlock (i_mutex *m)
{
  LeaveCriticalSection (&m->cs);
}

////////////////////////////////////////////////////////////
// Spinlock (Using Critical Section on Windows)
err_t
i_spinlock_create (i_spinlock *m, error *e)
{
  InitializeCriticalSection (&m->lock);
  return SUCCESS;
}

void
i_spinlock_free (i_spinlock *m)
{
  DeleteCriticalSection (&m->lock);
}

void
i_spinlock_lock (i_spinlock *m)
{
  EnterCriticalSection (&m->lock);
}

void
i_spinlock_unlock (i_spinlock *m)
{
  LeaveCriticalSection (&m->lock);
}

////////////////////////////////////////////////////////////
// RW Lock
err_t
i_rwlock_create (i_rwlock *rw, error *e)
{
  InitializeSRWLock (&rw->lock);
  return SUCCESS;
}

void
i_rwlock_free (i_rwlock *rw)
{
  // SRWLock doesn't need explicit cleanup
}

void
i_rwlock_rdlock (i_rwlock *rw)
{
  AcquireSRWLockShared (&rw->lock);
}

void
i_rwlock_wrlock (i_rwlock *rw)
{
  AcquireSRWLockExclusive (&rw->lock);
}

void
i_rwlock_unlock (i_rwlock *rw)
{
  // Note: Windows SRWLock requires different release functions
  // This is a simplification - in practice you'd need to track lock type
  ReleaseSRWLockExclusive (&rw->lock);
}

////////////////////////////////////////////////////////////
// Thread
err_t
i_thread_create (i_thread *t, void *(*start_routine) (void *), void *arg, error *e)
{
  HANDLE h = CreateThread (
      NULL,
      0,
      (LPTHREAD_START_ROUTINE)start_routine,
      arg,
      0,
      NULL);

  if (!handle_is_valid (h))
    {
      return error_causef (e, ERR_IO, "thread_create: Error %lu", GetLastError ());
    }

  t->handle = h;
  return SUCCESS;
}

err_t
i_thread_join (i_thread *t, error *e)
{
  if (WaitForSingleObject (t->handle, INFINITE) != WAIT_OBJECT_0)
    {
      return error_causef (e, ERR_IO, "thread_join: Error %lu", GetLastError ());
    }

  CloseHandle (t->handle);
  return SUCCESS;
}

////////////////////////////////////////////////////////////
// Condition Variable
err_t
i_cond_create (i_cond *c, error *e)
{
  InitializeConditionVariable (&c->cond);
  return SUCCESS;
}

void
i_cond_free (i_cond *c)
{
  // Condition variables don't need explicit cleanup
}

void
i_cond_wait (i_cond *c, i_mutex *m)
{
  SleepConditionVariableCS (&c->cond, &m->cs, INFINITE);
}

void
i_cond_signal (i_cond *c)
{
  WakeConditionVariable (&c->cond);
}

void
i_cond_broadcast (i_cond *c)
{
  WakeAllConditionVariable (&c->cond);
}

////////////////////////////////////////////////////////////
// TIMING

// Timer API - handle-based monotonic timer
err_t
i_timer_create (i_timer *timer, error *e)
{
  ASSERT (timer);

  if (!QueryPerformanceFrequency (&timer->frequency))
    {
      return error_causef (e, ERR_IO, "QueryPerformanceFrequency: Error %lu", GetLastError ());
    }

  if (!QueryPerformanceCounter (&timer->start))
    {
      return error_causef (e, ERR_IO, "QueryPerformanceCounter: Error %lu", GetLastError ());
    }

  return SUCCESS;
}

void
i_timer_free (i_timer *timer)
{
  ASSERT (timer);
  // No cleanup needed for Windows timers
}

u64
i_timer_now_ns (i_timer *timer)
{
  ASSERT (timer);

  LARGE_INTEGER now;
  QueryPerformanceCounter (&now);

  // Calculate elapsed ticks
  i64 elapsed = now.QuadPart - timer->start.QuadPart;

  // Convert to nanoseconds
  return (u64) ((elapsed * 1000000000LL) / timer->frequency.QuadPart);
}

u64
i_timer_now_us (i_timer *timer)
{
  return i_timer_now_ns (timer) / 1000ULL;
}

u64
i_timer_now_ms (i_timer *timer)
{
  return i_timer_now_ns (timer) / 1000000ULL;
}

f64
i_timer_now_s (i_timer *timer)
{
  return (f64)i_timer_now_ns (timer) / 1000000000.0;
}

// Legacy API (deprecated - use i_timer instead)
void
i_get_monotonic_time (struct timespec *ts)
{
  static LARGE_INTEGER frequency;
  static int initialized = 0;

  if (!initialized)
    {
      QueryPerformanceFrequency (&frequency);
      initialized = 1;
    }

  LARGE_INTEGER counter;
  QueryPerformanceCounter (&counter);

  // Convert to seconds and nanoseconds
  ts->tv_sec = (long)(counter.QuadPart / frequency.QuadPart);
  ts->tv_nsec = (long)(((counter.QuadPart % frequency.QuadPart) * 1000000000) / frequency.QuadPart);
}

////////////////////////////////////////////////////////////
// LOGGING

void
i_log_internal (
    const char *prefix,
    const char *color,
    const char *fmt,
    ...)
{
  va_list args;
  va_start (args, fmt);
  fprintf (stderr, "[%-8.8s]: ", prefix);
  vfprintf (stderr, fmt, args);
  va_end (args);
}
