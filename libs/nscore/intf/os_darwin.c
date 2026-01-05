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
 *   Implements os.h for macOS/Darwin platform-specific OS operations.
 */

// core
#include <numstore/core/assert.h>
#include <numstore/core/bounds.h>
#include <numstore/core/error.h>
#include <numstore/core/filenames.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/test/testing.h>

#include <backtrace.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>

// os
// system
#undef bool

static inline bool
fd_is_open (int fd)
{
  return fcntl (fd, F_GETFD) != -1 || errno != EBADF;
}

DEFINE_DBG_ASSERT (
    i_file, i_file, fp,
    {
      ASSERT (fp);
      ASSERT (fd_is_open (fp->fd));
    })

////////////////////////////////////////////////////////////
// OPEN / CLOSE
err_t
i_open_rw (i_file *dest, const char *fname, error *e)
{
  int fd = open (fname, O_RDWR | O_CREAT, 0644);

  if (fd == -1)
    {
      error_causef (e, ERR_IO, "open_rw %s: %s", fname, strerror (errno));
      return e->cause_code;
    }
  *dest = (i_file){ .fd = fd };

  DBG_ASSERT (i_file, dest);

  return SUCCESS;
}

err_t
i_open_r (i_file *dest, const char *fname, error *e)
{
  int fd = open (fname, O_RDONLY | O_CREAT, 0644);

  if (fd == -1)
    {
      error_causef (e, ERR_IO, "open_r %s: %s", fname, strerror (errno));
      return e->cause_code;
    }
  *dest = (i_file){ .fd = fd };

  DBG_ASSERT (i_file, dest);

  return SUCCESS;
}

err_t
i_open_w (i_file *dest, const char *fname, error *e)
{
  int fd = open (fname, O_WRONLY | O_CREAT, 0644);

  if (fd == -1)
    {
      error_causef (e, ERR_IO, "open_w %s: %s", fname, strerror (errno));
      return e->cause_code;
    }
  *dest = (i_file){ .fd = fd };

  DBG_ASSERT (i_file, dest);

  return SUCCESS;
}

err_t
i_close (i_file *fp, error *e)
{
  DBG_ASSERT (i_file, fp);
  int ret = close (fp->fd);
  if (ret)
    {
      return error_causef (e, ERR_IO, "close: %s", strerror (errno));
    }
  return SUCCESS;
}

err_t
i_fsync (i_file *fp, error *e)
{
  DBG_ASSERT (i_file, fp);
  int ret = fsync (fp->fd);
  if (ret)
    {
      return error_causef (e, ERR_IO, "fsync: %s", strerror (errno));
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

  ssize_t ret = pread (fp->fd, dest, n, (size_t)offset);

  if (ret < 0 && errno != EINTR)
    {
      return error_causef (e, ERR_IO, "pread: %s", strerror (errno));
    }

  return (i64)ret;
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
      /* Do read */
      ASSERT (n > nread);
      ssize_t _nread = pread (
          fp->fd,
          _dest + nread,
          n - nread,
          offset + nread);

      /* EOF */
      if (_nread == 0)
        {
          return (i64)nread;
        }

      /* Error */
      if (_nread < 0 && errno != EINTR)
        {
          return error_causef (e, ERR_IO, "pread: %s", strerror (errno));
        }

      nread += (i64)_nread;
    }

  ASSERT (nread == n);

  return nread;
}

err_t
i_pread_all_expect (i_file *fp, void *dest, u64 n, u64 offset, error *e)
{
  i64 ret = i_pread_all (fp, dest, n, offset, e);
  err_t_wrap (ret, e);

  if ((u64)ret != n)
    {
      return error_causef (e, ERR_CORRUPT, "Expected full pread");
    }

  return SUCCESS;
}

i64
i_pwrite_some (i_file *fp, const void *src, u64 n, u64 offset, error *e)
{
  DBG_ASSERT (i_file, fp);
  ASSERT (src);
  ASSERT (n > 0);

  ssize_t ret = pwrite (fp->fd, src, n, (size_t)offset);
  if (ret < 0 && errno != EINTR)
    {
      return error_causef (e, ERR_IO, "pread: %s", strerror (errno));
    }
  return (i64)ret;
}

err_t
i_pwrite_all (i_file *fp, const void *src, u64 n, u64 offset, error *e)
{
  DBG_ASSERT (i_file, fp);
  ASSERT (src);
  ASSERT (n > 0);

  u8 *_src = (u8 *)src;
  u64 nwrite = 0;

  while (nwrite < n)
    {
      /* Do write */
      ASSERT (n > nwrite);
      ssize_t _nwrite = pwrite (
          fp->fd,
          _src + nwrite,
          n - nwrite,
          offset + nwrite);

      /* Error */
      if (_nwrite < 0 && errno != EINTR)
        {
          return error_causef (e, ERR_IO, "pwrite: %s", strerror (errno));
        }

      nwrite += _nwrite;
    }

  ASSERT (nwrite == n);

  return SUCCESS;
}

static int
backtrace_callback (
    void *data,
    uintptr_t pc,
    const char *filename,
    int lineno,
    const char *function)
{
  if (filename)
    {
      printf ("%s:%d", basename (filename), lineno);
    }

  if (function)
    {
      printf ("(%s)", function);
    }
  else
    {
      printf ("(\?\?\?)");
    }

  printf ("\n");

  return 0;
}

static void
error_callback (void *data, const char *msg, int errnum)
{
  fprintf (stderr, "Backtrace error: %s (%d)\n", msg, errnum);
}

void
i_print_stack_trace (void)
{
  struct backtrace_state *state;

  // Initialize backtrace state (do this once, ideally at program start)
  state = backtrace_create_state (NULL, 1, error_callback, NULL);

  if (state == NULL)
    {
      fprintf (stderr, "Failed to create backtrace state\n");
      return;
    }

  printf ("Stack trace:\n");
  backtrace_full (state, 0, backtrace_callback, error_callback, NULL);

  fflush (stdout);
}

//////////////// Stream Read / Write

i64
i_read_some (i_file *fp, void *dest, u64 nbytes, error *e)
{
  DBG_ASSERT (i_file, fp);
  ASSERT (dest);
  ASSERT (nbytes > 0);

  i_log_trace ("Trying to read: %llu bytes\n", nbytes);
  ssize_t ret = read (fp->fd, dest, nbytes);
  i_log_trace ("Read: %ld bytes\n", ret);

  if (ret < 0)
    {
      if (errno == EINTR || errno == EWOULDBLOCK)
        {
          i_log_trace ("Read got errno: %d - ok\n", errno);
          return 0;
        }
      return error_causef (e, ERR_IO, "read: %s", strerror (errno));
    }
  return (i64)ret;
}

i64
i_read_all (i_file *fp, void *dest, u64 nbytes, error *e)
{
  DBG_ASSERT (i_file, fp);
  ASSERT (dest);
  ASSERT (nbytes > 0);

  u8 *_dest = (u8 *)dest;
  u64 nread = 0;

  while (nread < nbytes)
    {
      /* Do read */
      ASSERT (nbytes > nread);
      ssize_t _nread = read (
          fp->fd,
          _dest + nread,
          nbytes - nread);

      /* EOF */
      if (_nread == 0)
        {
          return (i64)nread;
        }

      /* Error */
      if (_nread < 0)
        {
          if (errno == EINTR || errno == EWOULDBLOCK)
            {
              return 0;
            }
          return error_causef (e, ERR_IO, "read: %s", strerror (errno));
        }

      nread += (i64)_nread;
    }

  ASSERT (nread == nbytes);

  return nread;
}

i64
i_read_all_expect (i_file *fp, void *dest, u64 nbytes, error *e)
{
  i64 ret = i_read_all (fp, dest, nbytes, e);
  err_t_wrap (ret, e);

  if ((u64)ret != nbytes)
    {
      return error_causef (e, ERR_CORRUPT, "Expected full read");
    }

  return SUCCESS;
}

i64
i_write_some (i_file *fp, const void *src, u64 nbytes, error *e)
{
  DBG_ASSERT (i_file, fp);
  ASSERT (src);
  ASSERT (nbytes > 0);

  i_log_trace ("Trying to write: %llu bytes\n", nbytes);
  ssize_t ret = write (fp->fd, src, nbytes);
  i_log_trace ("Written: %lu bytes\n", ret);
  if (ret < 0 && errno != EINTR)
    {
      return error_causef (e, ERR_IO, "write: %s", strerror (errno));
    }
  return (i64)ret;
}

err_t
i_write_all (i_file *fp, const void *src, u64 nbytes, error *e)
{
  DBG_ASSERT (i_file, fp);
  ASSERT (src);
  ASSERT (nbytes > 0);

  u8 *_src = (u8 *)src;
  u64 nwrite = 0;

  while (nwrite < nbytes)
    {
      /* Do read */
      ASSERT (nbytes > nwrite);

      ssize_t _nwrite = write (
          fp->fd,
          _src + nwrite,
          nbytes - nwrite);

      /* Error */
      if (_nwrite < 0 && errno != EINTR)
        {
          return error_causef (e, ERR_IO, "write: %s", strerror (errno));
        }

      nwrite += _nwrite;
    }

  ASSERT (nwrite == nbytes);

  return SUCCESS;
}

////////////////////////////////////////////////////////////
// OTHERS
err_t
i_truncate (i_file *fp, u64 bytes, error *e)
{
  if (ftruncate (fp->fd, bytes) == -1)
    {
      return error_causef (e, ERR_IO, "truncate: %s", strerror (errno));
    }

  return 0;
}

i64
i_file_size (i_file *fp, error *e)
{
  struct stat st;
  if (fstat (fp->fd, &st) == -1)
    {
      error_causef (e, ERR_IO, "fstat: %s", strerror (errno));
      return e->cause_code;
    }
  return (i64)st.st_size;
}

err_t
i_remove_quiet (const char *fname, error *e)
{
  int ret = remove (fname);

  if (ret && errno != ENOENT)
    {
      error_causef (e, ERR_IO, "remove: %s", strerror (errno));
      return e->cause_code;
    }

  return SUCCESS;
}

err_t
i_mkstemp (i_file *dest, char *tmpl, error *e)
{
  int fd = mkstemp (tmpl);
  if (fd == -1)
    {
      error_causef (e, ERR_IO, "mkstemp: %s", strerror (errno));
      return e->cause_code;
    }

  dest->fd = fd;
  return SUCCESS;
}

err_t
i_unlink (const char *name, error *e)
{
  if (unlink (name))
    {
      error_causef (e, ERR_IO, "unlink: %s", strerror (errno));
      return e->cause_code;
    }
  return SUCCESS;
}

i64
i_seek (i_file *fp, u64 offset, seek_t whence, error *e)
{
  int seek;
  switch (whence)
    {
    case I_SEEK_SET:
      {
        seek = SEEK_SET;
        break;
      }
    case I_SEEK_CUR:
      {
        seek = SEEK_CUR;
        break;
      }
    case I_SEEK_END:
      {
        seek = SEEK_END;
        break;
      }
    default:
      {
        UNREACHABLE ();
      }
    }

  errno = 0;
  off_t ret = lseek (fp->fd, offset, seek);
  if (ret == (off_t)-1)
    {
      error_causef (e, ERR_IO, "lseek: %s", strerror (errno));
      return e->cause_code;
    }

  return (u64)ret;
}

////////////////////////////////////////////////////////////
// WRAPPERS
err_t
i_access_rw (const char *fname, error *e)
{
  if (access (fname, F_OK | W_OK | R_OK))
    {
      error_causef (e, ERR_IO, "access: %s", strerror (errno));
      return e->cause_code;
    }
  return SUCCESS;
}

bool
i_exists_rw (const char *fname)
{
  if (access (fname, F_OK | W_OK | R_OK))
    {
      return false;
    }
  return true;
}

err_t
i_touch (const char *fname, error *e)
{
  ASSERT (fname);

  i_file fd = { 0 };
  err_t_wrap (i_open_rw (&fd, fname, e), e);
  err_t_wrap (i_close (&fd, e), e);

  return SUCCESS;
}

////////////////////////////////////////////////////////////
// MEMORY
void *
i_malloc (u32 nelem, u32 size, error *e)
{
  ASSERT (nelem > 0);
  ASSERT (size > 0);

  u32 bytes;
  if (!SAFE_MUL_U32 (&bytes, nelem, size))
    {
      error_causef (e, ERR_NOMEM, "cannot allocate %d elements of size %d", nelem, size);
      return NULL;
    }

  errno = 0;
  void *ret = malloc ((size_t)bytes);
  if (ret == NULL)
    {
      if (errno == ENOMEM)
        {
          error_causef (e, ERR_NOMEM,
                        "malloc failed to allocate %d elements of size %d: %s",
                        nelem, size, strerror (errno));
        }
      else
        {
          error_causef (e, ERR_NOMEM, "malloc failed: %s", strerror (errno));
        }
    }
  return ret;
}

void *
i_calloc (u32 nelem, u32 size, error *e)
{
  ASSERT (nelem > 0);
  ASSERT (size > 0);

  u32 bytes = 0;
  if (!SAFE_MUL_U32 (&bytes, nelem, size))
    {
      error_causef (e, ERR_NOMEM, "cannot allocate %d elements of size %d", nelem, size);
      return NULL;
    }

  ASSERT (bytes > 0);

  errno = 0;
  void *ret = calloc ((size_t)nelem, (size_t)size);
  if (ret == NULL)
    {
      if (errno == ENOMEM)
        {
          error_causef (
              e, ERR_NOMEM,
              "calloc failed to allocate %d elements of size %d: %s", nelem, size, strerror (errno));
        }
      else
        {
          error_causef (e, ERR_NOMEM, "calloc failed: %s", strerror (errno));
        }
    }
  return ret;
}

// =======================================================
// Core realloc wrapper (used by all)
// =======================================================
static inline void *
i_realloc (void *ptr, u32 nelem, u32 size, error *e)
{
  ASSERT (nelem > 0);
  ASSERT (size > 0);

  u32 bytes = 0;
  {
    bool ok = SAFE_MUL_U32 (&bytes, nelem, size);
    ASSERT (ok);
    if (!ok)
      {
        error_causef (e, ERR_NOMEM, "i_realloc: overflow %u * %u", nelem, size);
        return NULL;
      }
  }

  errno = 0;
  void *ret = realloc (ptr, (size_t)bytes);
  if (ret == NULL)
    {
      error_causef (e, ERR_NOMEM, "realloc(%u bytes) failed: %s", bytes, strerror (errno));
      return NULL;
    }
  return ret;
}

#ifndef NTEST
TEST (TT_UNIT, i_realloc_basic)
{
  error e = error_create ();
  u32 *a = i_realloc (NULL, 10, sizeof *a, &e); // behaves like malloc
  test_fail_if_null (a);

  for (u32 i = 0; i < 10; i++)
    {
      a[i] = i;
    }

  u32 *b = i_realloc (a, 20, sizeof *b, &e);
  test_fail_if_null (b);

  for (u32 i = 0; i < 10; i++)
    {
      test_assert (b[i] == i);
    }
  i_free (b);
}
#endif

// =======================================================
// RIGHT REALLOC (no shifting) — caller passes old_nelem
// =======================================================
void *
i_realloc_right (void *ptr, u32 old_nelem, u32 new_nelem, u32 size, error *e)
{
  ASSERT (size > 0);
  if (ptr == NULL)
    {
      ASSERT (old_nelem == 0);
    }
  ASSERT (new_nelem > 0);

  return i_realloc (ptr, new_nelem, size, e);
}

#ifndef NTEST
TEST (TT_UNIT, i_realloc_right)
{
  error e = error_create ();
  u32 *a = i_malloc (10, sizeof *a, &e);
  test_fail_if_null (a);

  for (u32 i = 0; i < 10; i++)
    {
      a[i] = i;
    }

  a = i_realloc_right (a, 10, 20, sizeof *a, &e);
  test_fail_if_null (a);

  for (u32 i = 0; i < 10; i++)
    {
      test_assert (a[i] == i);
    }

  // shrink path
  a = i_realloc_right (a, 20, 10, sizeof *a, &e);
  test_fail_if_null (a);

  for (u32 i = 0; i < 10; i++)
    {
      test_assert (a[i] == i);
    }
  i_free (a);
}
#endif

// LEFT REALLOC (grow prepends space; shrink drops left)
void *
i_realloc_left (void *ptr, u32 old_nelem, u32 new_nelem, u32 size, error *e)
{
  ASSERT (size > 0);
  if (ptr == NULL)
    {
      ASSERT (old_nelem == 0);
    }
  ASSERT (new_nelem > 0);

  if (old_nelem == new_nelem)
    {
      return ptr;
    }

  u32 old_bytes32 = 0, new_bytes32 = 0;
  {
    bool ok_old = SAFE_MUL_U32 (&old_bytes32, old_nelem, size);
    ASSERT (ok_old);
    bool ok_new = SAFE_MUL_U32 (&new_bytes32, new_nelem, size);
    ASSERT (ok_new);
    if (!ok_old || !ok_new)
      {
        error_causef (e, ERR_NOMEM, "i_realloc_left: overflow");
        return NULL;
      }
  }

  size_t old_bytes = (size_t)old_bytes32;
  size_t new_bytes = (size_t)new_bytes32;

  if (ptr == NULL)
    {
      return i_realloc (NULL, new_nelem, size, e);
    }

  if (new_bytes < old_bytes)
    {
      // keep the last new_bytes bytes; move them to start
      size_t keep = new_bytes;
      size_t shift = old_bytes - keep;
      memmove (ptr, (char *)ptr + shift, keep);
      return i_realloc (ptr, new_nelem, size, e);
    }
  else
    {
      // prepend = new space on the left
      size_t prepend = new_bytes - old_bytes;

      void *ret = i_realloc (ptr, new_nelem, size, e);
      if (ret == NULL)
        {
          return NULL;
        }

      if (old_bytes > 0)
        {
          memmove ((char *)ret + prepend, ret, old_bytes);
        }
      return ret;
    }
}

#ifndef NTEST
TEST (TT_UNIT, i_realloc_left)
{
  error e = error_create ();
  u32 *a = i_malloc (10, sizeof *a, &e);
  test_fail_if_null (a);

  for (u32 i = 0; i < 10; i++)
    {
      a[i] = i;
    }

  // grow-left: old payload should appear starting at index 10
  a = i_realloc_left (a, 10, 20, sizeof *a, &e);
  test_fail_if_null (a);

  for (u32 i = 0; i < 10; i++)
    {
      test_assert (a[10 + i] == i);
    }

  // shrink-left: keep the *last* 10 elements moved to start
  a = i_realloc_left (a, 20, 10, sizeof *a, &e);
  test_fail_if_null (a);

  for (u32 i = 0; i < 10; i++)
    {
      test_assert (a[i] == i); // after previous state, kept tail [10..19] which were [0..9]
    }
  i_free (a);
}
#endif

// =======================================================
// C-Right: zero-fill new tail
// =======================================================
void *
i_crealloc_right (void *ptr, u32 old_nelem, u32 new_nelem, u32 size, error *e)
{
  ASSERT (size > 0);
  if (ptr == NULL)
    {
      ASSERT (old_nelem == 0);
    }
  ASSERT (new_nelem > 0);

  u32 old_bytes32 = 0, new_bytes32 = 0;
  {
    bool ok_old = SAFE_MUL_U32 (&old_bytes32, old_nelem, size);
    ASSERT (ok_old);
    bool ok_new = SAFE_MUL_U32 (&new_bytes32, new_nelem, size);
    ASSERT (ok_new);
    if (!ok_old || !ok_new)
      {
        error_causef (e, ERR_NOMEM, "i_crealloc_right: overflow");
        return NULL;
      }
  }

  size_t old_bytes = (size_t)old_bytes32;
  size_t new_bytes = (size_t)new_bytes32;

  void *ret = i_realloc (ptr, new_nelem, size, e);
  if (ret == NULL)
    {
      return NULL;
    }

  if (new_bytes > old_bytes)
    {
      memset ((char *)ret + old_bytes, 0, new_bytes - old_bytes);
    }
  return ret;
}

#ifndef NTEST
TEST (TT_UNIT, i_crealloc_right)
{
  error e = error_create ();
  u32 *a = i_malloc (10, sizeof *a, &e);
  test_fail_if_null (a);

  for (u32 i = 0; i < 10; i++)
    {
      a[i] = i;
    }

  a = i_crealloc_right (a, 10, 20, sizeof *a, &e);
  test_fail_if_null (a);

  for (u32 i = 0; i < 10; i++)
    {
      test_assert (a[i] == i);
    }
  for (u32 i = 10; i < 20; i++)
    {
      test_assert (a[i] == 0);
    }

  // shrink keeps prefix
  a = i_crealloc_right (a, 20, 10, sizeof *a, &e);
  test_fail_if_null (a);
  for (u32 i = 0; i < 10; i++)
    {
      test_assert (a[i] == i);
    }
  i_free (a);
}
#endif

// =======================================================
// C-Left: zero-fill new head; shrink drops head
// =======================================================
void *
i_crealloc_left (void *ptr, u32 old_nelem, u32 new_nelem, u32 size, error *e)
{
  ASSERT (size > 0);
  if (ptr == NULL)
    {
      ASSERT (old_nelem == 0);
    }
  ASSERT (new_nelem > 0);

  if (old_nelem == new_nelem)
    {
      return ptr;
    }

  u32 old_bytes32 = 0, new_bytes32 = 0;
  {
    bool ok_old = SAFE_MUL_U32 (&old_bytes32, old_nelem, size);
    ASSERT (ok_old);
    bool ok_new = SAFE_MUL_U32 (&new_bytes32, new_nelem, size);
    ASSERT (ok_new);
    if (!ok_old || !ok_new)
      {
        error_causef (e, ERR_NOMEM, "i_crealloc_left: overflow");
        return NULL;
      }
  }

  size_t old_bytes = (size_t)old_bytes32;
  size_t new_bytes = (size_t)new_bytes32;

  if (ptr == NULL)
    {
      void *ret = i_realloc (NULL, new_nelem, size, e);
      if (ret != NULL)
        {
          memset (ret, 0, new_bytes);
        }
      return ret;
    }

  if (new_bytes < old_bytes)
    {
      size_t keep = new_bytes;
      size_t shift = old_bytes - keep;
      memmove (ptr, (char *)ptr + shift, keep);
      return i_realloc (ptr, new_nelem, size, e);
    }
  else
    {
      size_t prepend = new_bytes - old_bytes;

      void *ret = i_realloc (ptr, new_nelem, size, e);
      if (ret == NULL)
        {
          return NULL;
        }

      if (old_bytes > 0)
        {
          memmove ((char *)ret + prepend, ret, old_bytes);
        }
      memset (ret, 0, prepend);
      return ret;
    }
}

#ifndef NTEST
TEST (TT_UNIT, i_crealloc_left)
{
  error e = error_create ();
  u32 *a = i_malloc (10, sizeof *a, &e);
  test_fail_if_null (a);

  for (u32 i = 0; i < 10; i++)
    {
      a[i] = i;
    }

  // grow-left: zeros in new head; old payload shifted right
  a = i_crealloc_left (a, 10, 20, sizeof *a, &e);
  test_fail_if_null (a);

  for (u32 i = 0; i < 10; i++)
    {
      test_assert (a[i] == 0);
    }
  for (u32 i = 0; i < 10; i++)
    {
      test_assert (a[10 + i] == i);
    }

  // shrink-left: drop head, keep last 10 at start
  a = i_crealloc_left (a, 20, 10, sizeof *a, &e);
  test_fail_if_null (a);

  for (u32 i = 0; i < 10; i++)
    {
      test_assert (a[i] == i);
    }
  i_free (a);
}
#endif

void
i_free (void *ptr)
{
  ASSERT (ptr);
  free (ptr);
}

////////////////// Mutex

struct i_mutex_s
{
  pthread_mutex_t mutex;
};

err_t
i_mutex_create (i_mutex *dest, error *e)
{
  errno = 0;
#ifndef NDEBUG
  pthread_mutexattr_t attr;

  /* I just don't want to handle errors for debug code */
  int r1 = pthread_mutexattr_init (&attr);
  ASSERT (!r1);

  r1 = pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_ERRORCHECK);
  ASSERT (!r1);

  int r2 = pthread_mutex_init (&dest->m, NULL);

  r1 = pthread_mutexattr_destroy (&attr);
  ASSERT (!r1);
  if (r2)
#else
  if (pthread_mutex_init (&dest->m, NULL))
#endif
    {
      switch (errno)
        {
        case EAGAIN:
          {
            return error_causef (
                e, ERR_IO,
                "Failed to initialize mutex: %s",
                strerror (errno));
          }
        case ENOMEM:
          {
            return error_causef (
                e, ERR_NOMEM,
                "Failed to initialize mutex: %s",
                strerror (errno));
          }
        case EPERM:
          {
            i_log_error ("mutex create: insufficient permissions: %s\n", strerror (errno));
            UNREACHABLE ();
          }
        default:
          {
            UNREACHABLE ();
          }
        }
    }

  return SUCCESS;
}

void
i_mutex_free (i_mutex *m)
{
  ASSERT (m);

  errno = 0;
  if (pthread_mutex_destroy (&m->m))
    {
      switch (errno)
        {
        case EBUSY:
          {
            i_log_error ("Mutex is locked! %s\n", strerror (errno));
            UNREACHABLE ();
          }
        case EINVAL:
          {
            i_log_error ("Invalid Mutex! %s\n", strerror (errno));
            UNREACHABLE ();
          }
        default:
          {
            UNREACHABLE ();
          }
        }
    }
}

void
i_mutex_lock (i_mutex *m)
{
  ASSERT (m);

  errno = 0;
  if (pthread_mutex_lock (&m->m))
    {
      switch (errno)
        {
        case EINVAL:
          {
            i_log_error ("lock: Failed to lock mutex! %s\n", strerror (errno));
            UNREACHABLE ();
          }
        case EAGAIN:
          {
            i_log_error ("Recursive locks are not allowed: %s\n", strerror (errno));
            UNREACHABLE ();
          }
        case EDEADLK:
          {
            i_log_error ("lock: Deadlock detected! %s\n", strerror (errno));
            UNREACHABLE ();
          }
        default:
          {
            i_log_error ("lock: Unknown error detected! %s\n", strerror (errno));
            UNREACHABLE ();
          }
        }
    }
}

void
i_mutex_unlock (i_mutex *m)
{
  ASSERT (m);

  errno = 0;
  if (pthread_mutex_unlock (&m->m))
    {
      switch (errno)
        {
        case EINVAL:
          {
            i_log_error ("unlock: Failed to unlock mutex! %s\n", strerror (errno));
            UNREACHABLE ();
          }
        case EAGAIN:
          {
            i_log_error ("unclock: Recursive locks are not allowed: %s\n", strerror (errno));
            UNREACHABLE ();
          }
        case EPERM:
          {
            i_log_error ("unlock: current thread doesn't own this mutex: %s\n", strerror (errno));
            UNREACHABLE ();
          }
        default:
          {
            i_log_error ("unlock: Unknown error detected! %s\n", strerror (errno));
            UNREACHABLE ();
          }
        }
    }
}

////////////////// Spin Lock

err_t
i_spinlock_create (i_spinlock *dest, error *e)
{
  ASSERT (dest);
  errno = 0;

  // PTHREAD_PROCESS_PRIVATE means the spinlock is only shared between threads
  // of the same process (not across processes)
  if (pthread_spin_init (&dest->lock, PTHREAD_PROCESS_PRIVATE))
    {
      switch (errno)
        {
        case EAGAIN:
          {
            return error_causef (
                e, ERR_IO,
                "Failed to initialize spinlock: %s",
                strerror (errno));
          }
        case ENOMEM:
          {
            return error_causef (
                e, ERR_NOMEM,
                "Failed to initialize spinlock: %s",
                strerror (errno));
          }
        case EINVAL:
          {
            i_log_error ("spinlock create: invalid pshared value: %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        default:
          {
            UNREACHABLE ();
          }
        }
    }
  return SUCCESS;
}

void
i_spinlock_free (i_spinlock *m)
{
  ASSERT (m);
  errno = 0;

  if (pthread_spin_destroy (&m->lock))
    {
      switch (errno)
        {
        case EBUSY:
          {
            i_log_error ("Spinlock is locked! %s\n", strerror (errno));
            UNREACHABLE ();
          }
        case EINVAL:
          {
            i_log_error ("Invalid Spinlock! %s\n", strerror (errno));
            UNREACHABLE ();
          }
        default:
          {
            UNREACHABLE ();
          }
        }
    }
}

void
i_spinlock_lock (i_spinlock *m)
{
  ASSERT (m);
  errno = 0;

  if (pthread_spin_lock (&m->lock))
    {
      switch (errno)
        {
        case EINVAL:
          {
            i_log_error ("spinlock lock: Invalid spinlock! %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        case EDEADLK:
          {
            i_log_error ("spinlock lock: Deadlock detected! %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        default:
          {
            i_log_error ("spinlock lock: Unknown error detected! %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        }
    }
}

void
i_spinlock_unlock (i_spinlock *m)
{
  ASSERT (m);
  errno = 0;

  if (pthread_spin_unlock (&m->lock))
    {
      switch (errno)
        {
        case EINVAL:
          {
            i_log_error ("spinlock unlock: Invalid spinlock! %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        case EPERM:
          {
            i_log_error ("spinlock unlock: current thread doesn't own this spinlock: %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        default:
          {
            i_log_error ("spinlock unlock: Unknown error detected! %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        }
    }
}

////////////////// RW Lock

err_t
i_rwlock_create (i_rwlock *dest, error *e)
{
  errno = 0;
#ifndef NDEBUG
  pthread_rwlockattr_t attr;
  int r1 = pthread_rwlockattr_init (&attr);
  ASSERT (!r1);
  // Set prefer-writer or other attributes if needed
  // pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
  int r2 = pthread_rwlock_init (&dest->rwlock, &attr);
  r1 = pthread_rwlockattr_destroy (&attr);
  ASSERT (!r1);
  if (r2)
#else
  if (pthread_rwlock_init (&dest->rwlock, NULL))
#endif
    {
      switch (errno)
        {
        case EAGAIN:
          {
            return error_causef (
                e, ERR_IO,
                "Failed to initialize rwlock: %s",
                strerror (errno));
          }
        case ENOMEM:
          {
            return error_causef (
                e, ERR_NOMEM,
                "Failed to initialize rwlock: %s",
                strerror (errno));
          }
        case EPERM:
          {
            i_log_error ("rwlock create: insufficient permissions: %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        case EINVAL:
          {
            i_log_error ("rwlock create: invalid attributes: %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        default:
          {
            UNREACHABLE ();
          }
        }
    }
  return SUCCESS;
}

void
i_rwlock_free (i_rwlock *m)
{
  ASSERT (m);
  errno = 0;
  if (pthread_rwlock_destroy (&m->rwlock))
    {
      switch (errno)
        {
        case EBUSY:
          {
            i_log_error ("RWLock is locked! %s\n", strerror (errno));
            UNREACHABLE ();
          }
        case EINVAL:
          {
            i_log_error ("Invalid RWLock! %s\n", strerror (errno));
            UNREACHABLE ();
          }
        default:
          {
            UNREACHABLE ();
          }
        }
    }
}

void
i_rwlock_s_lock (i_rwlock *m)
{
  ASSERT (m);
  errno = 0;
  if (pthread_rwlock_rdlock (&m->rwlock))
    {
      switch (errno)
        {
        case EBUSY:
          {
            i_log_error ("s_lock: Already locked for reading: %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        case EDEADLK:
          {
            i_log_error ("s_lock: Deadlock detected! %s\n", strerror (errno));
            UNREACHABLE ();
          }
        default:
          {
            i_log_error ("s_lock: Unknown error detected! %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        }
    }
}

void
i_rwlock_x_lock (i_rwlock *m)
{
  ASSERT (m);
  errno = 0;
  if (pthread_rwlock_wrlock (&m->rwlock))
    {
      switch (errno)
        {
        case EDEADLK:
          {
            i_log_error ("x_lock: Deadlock detected! %s\n", strerror (errno));
            UNREACHABLE ();
          }
        default:
          {
            i_log_error ("x_lock: Unknown error detected! %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        }
    }
}

bool
i_rwlock_s_try_lock (i_rwlock *m)
{
  ASSERT (m);
  errno = 0;
  int result = pthread_rwlock_tryrdlock (&m->rwlock);

  if (result == 0)
    {
      return true; // Lock acquired
    }
  else if (result == EBUSY)
    {
      return false; // Lock not available, but not an error
    }
  else
    {
      switch (errno)
        {
        default:
          {
            i_log_error ("s_try_lock: Unknown error detected! %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        }
    }
}

bool
i_rwlock_x_try_lock (i_rwlock *m)
{
  ASSERT (m);
  errno = 0;
  int result = pthread_rwlock_trywrlock (&m->rwlock);

  if (result == 0)
    {
      return true; // Lock acquired
    }
  else if (result == EBUSY)
    {
      return false; // Lock not available, but not an error
    }
  else
    {
      switch (errno)
        {
        default:
          {
            i_log_error ("x_try_lock: Unknown error detected! %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        }
    }
}

void
i_rwlock_s_unlock (i_rwlock *m)
{
  ASSERT (m);
  errno = 0;
  if (pthread_rwlock_unlock (&m->rwlock))
    {
      switch (errno)
        {
        case EINVAL:
          {
            i_log_error ("s_unlock: Failed to unlock rwlock! %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        case EPERM:
          {
            i_log_error ("s_unlock: current thread doesn't own this lock: %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        default:
          {
            i_log_error ("s_unlock: Unknown error detected! %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        }
    }
}

void
i_rwlock_x_unlock (i_rwlock *m)
{
  ASSERT (m);
  errno = 0;
  if (pthread_rwlock_unlock (&m->rwlock))
    {
      switch (errno)
        {
        case EINVAL:
          {
            i_log_error ("x_unlock: Failed to unlock rwlock! %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        case EPERM:
          {
            i_log_error ("x_unlock: current thread doesn't own this lock: %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        default:
          {
            i_log_error ("x_unlock: Unknown error detected! %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        }
    }
}

////////////////// Condition Variable

err_t
i_cond_create (i_cond *c, error *e)
{
  ASSERT (c);

#ifndef NDEBUG
  pthread_condattr_t attr;

  /* I just don't want to handle errors for debug code */
  int r1 = pthread_condattr_init (&attr);
  ASSERT (r1 == 0);

  int r2 = pthread_cond_init (&c->c, &attr);

  r1 = pthread_condattr_destroy (&attr);
  ASSERT (r1 == 0);

  if (r2)
#else
  if (pthread_cond_init (&c->c, NULL))
#endif
    {
      switch (errno)
        {
        case EAGAIN:
          {
            return error_causef (
                e, ERR_IO,
                "cond_create: system limit reached: %s",
                strerror (errno));
          }

        case ENOMEM:
          {
            return error_causef (
                e, ERR_NOMEM,
                "cond_create: not enough memory: %s",
                strerror (errno));
          }

        case EBUSY:
          {
            i_log_error ("cond_create: cond already initialized: %s\n", strerror (errno));
            UNREACHABLE ();
          }

        case EINVAL:
          {
            i_log_error ("cond_create: invalid attributes or cond: %s\n", strerror (errno));
            UNREACHABLE ();
          }

        default:
          {
            i_log_error ("cond_create: unknown error: %s\n", strerror (errno));
            UNREACHABLE ();
          }
        }
    }

  return SUCCESS;
}

void
i_cond_free (i_cond *c)
{
  ASSERT (c);

  errno = 0;
  if (pthread_cond_destroy (&c->c))
    {
      switch (errno)
        {
        case EBUSY:
          {
            i_log_error ("cond_free: cond has active waiters: %s\n", strerror (errno));
            UNREACHABLE ();
          }

        case EINVAL:
          {
            i_log_error ("cond_free: invalid or uninitialized cond: %s\n", strerror (errno));
            UNREACHABLE ();
          }

        default:
          {
            i_log_error ("cond_free: unknown error: %s\n", strerror (errno));
            UNREACHABLE ();
          }
        }
    }
}

void
i_cond_wait (i_cond *c, i_mutex *m)
{
  ASSERT (c);
  ASSERT (m);

  errno = 0;
  if (pthread_cond_wait (&c->c, &m->m))
    {
      switch (errno)
        {
        case EINVAL:
          {
            i_log_error ("cond_wait: invalid cond or mutex: %s\n", strerror (errno));
            UNREACHABLE ();
          }

        case EPERM:
          {
            i_log_error ("cond_wait: mutex not owned by thread: %s\n", strerror (errno));
            UNREACHABLE ();
          }

        default:
          {
            i_log_error ("cond_wait: unknown error: %s\n", strerror (errno));
            UNREACHABLE ();
          }
        }
    }
}

void
i_cond_signal (i_cond *c)
{
  ASSERT (c);

  errno = 0;
  if (pthread_cond_signal (&c->c))
    {
      switch (errno)
        {
        case EINVAL:
          {
            i_log_error ("cond_signal: invalid or uninitialized cond: %s\n", strerror (errno));
            UNREACHABLE ();
          }

        default:
          {
            i_log_error ("cond_signal: unknown error: %s\n", strerror (errno));
            UNREACHABLE ();
          }
        }
    }
}

void
i_cond_broadcast (i_cond *c)
{
  ASSERT (c);

  errno = 0;
  if (pthread_cond_broadcast (&c->c))
    {
      switch (errno)
        {
        case EINVAL:
          {
            i_log_error ("cond_broadcast: invalid or uninitialized cond: %s\n", strerror (errno));
            UNREACHABLE ();
          }

        default:
          {
            i_log_error ("cond_broadcast: unknown error: %s\n", strerror (errno));
            UNREACHABLE ();
          }
        }
    }
}

////////////////// Condition Variable

err_t
i_thread_create (i_thread *dest,
                 void *(*func) (void *),
                 void *context,
                 error *e)
{
  ASSERT (dest);

#ifndef NDEBUG
  pthread_attr_t attr;
  int r1 = pthread_attr_init (&attr);
  ASSERT (!r1);

  // Examples:
  // pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  // pthread_attr_setstacksize(&attr, 1 << 20);
  // pthread_attr_setguardsize(&attr, 4096);
  // pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  // pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
  // pthread_attr_setschedparam(&attr, &sched_param);

  int r2 = pthread_create (&dest->t, &attr, func, context);

  r1 = pthread_attr_destroy (&attr);
  ASSERT (!r1);
  if (r2)
#else
  if (pthread_create (&dest->t, NULL, func, context))
#endif
    {
      switch (errno)
        {
        case EAGAIN:
          {
            return error_causef (e, ERR_IO, "Failed to create thread: %s", strerror (errno));
          }
        case EINVAL:
          {
            i_log_error ("thread create: invalid attributes: %s\n", strerror (errno));
            UNREACHABLE ();
          }
        case EPERM:
          {
            i_log_error ("thread create: insufficient permissions: %s\n", strerror (errno));
            UNREACHABLE ();
          }
        default:
          {
            UNREACHABLE ();
          }
        }
    }

  return SUCCESS;
}

void
i_thread_join (i_thread *t, void **retval)
{
  ASSERT (t);

  int r = pthread_join (t->t, retval);

  if (r != 0)
    {
      switch (r)
        {
        case EDEADLK:
          {
            i_log_error ("thread join: deadlock detected (joining self?) %s\n",
                         strerror (r));
            UNREACHABLE ();
          }
        case EINVAL:
          {
            i_log_error ("thread join: thread not joinable %s\n",
                         strerror (r));
            UNREACHABLE ();
          }
        case ESRCH:
          {
            i_log_error ("thread join: no such thread %s\n",
                         strerror (r));
            UNREACHABLE ();
          }
        default:
          {
            UNREACHABLE ();
          }
        }
    }
}

void
i_thread_cancel (i_thread *t)
{
  ASSERT (t);

  int r = pthread_cancel (t->t);
  if (r != 0)
    {
      switch (r)
        {
        case ESRCH:
          {
            i_log_error ("thread cancel: no such thread %s\n", strerror (r));
            UNREACHABLE ();
          }
        default:
          {
            UNREACHABLE ();
          }
        }
    }
}

u64
get_available_threads (void)
{
  long ret = sysconf (_SC_NPROCESSORS_ONLN);
  ASSERT (ret > 0);
  return (u64)ret;
}

////////////////////////////////////////////////////////////
// TIMING

// Timer API - handle-based monotonic timer
err_t
i_timer_create (i_timer *timer, error *e)
{
  ASSERT (timer);

  // Get timebase info for mach_absolute_time conversion
  mach_timebase_info_data_t timebase_info;
  if (mach_timebase_info (&timebase_info) != KERN_SUCCESS)
    {
      return error_causef (e, ERR_IO, "mach_timebase_info failed");
    }

  timer->numer = timebase_info.numer;
  timer->denom = timebase_info.denom;
  timer->start = mach_absolute_time ();

  return SUCCESS;
}

void
i_timer_free (i_timer *timer)
{
  ASSERT (timer);
  // No cleanup needed for Darwin timers
}

u64
i_timer_now_ns (i_timer *timer)
{
  ASSERT (timer);

  u64 now = mach_absolute_time ();
  u64 elapsed = now - timer->start;

  // Convert to nanoseconds
  return elapsed * timer->numer / timer->denom;
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
  // Use mach_absolute_time() for high-resolution timing on Darwin (macOS/iOS)
  static mach_timebase_info_data_t timebase_info;
  static int initialized = 0;

  if (!initialized)
    {
      mach_timebase_info (&timebase_info);
      initialized = 1;
    }

  uint64_t mach_time = mach_absolute_time ();

  // Convert to nanoseconds
  uint64_t nanoseconds = mach_time * timebase_info.numer / timebase_info.denom;

  // Convert to timespec (seconds and nanoseconds)
  ts->tv_sec = (long)(nanoseconds / 1000000000ULL);
  ts->tv_nsec = (long)(nanoseconds % 1000000000ULL);
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
  fprintf (stderr, "%s[%-8.8s]: ", color, prefix);
  vfprintf (stderr, fmt, args);
  fprintf (stderr, "%s", RESET);
  // fflush (stderr);
  va_end (args);
}
