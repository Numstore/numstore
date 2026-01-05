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
 *   Darwin file operations implementation
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

////////////////////////////////////////////////////////////
// File Stream

err_t
i_stream_open_rw (i_stream *dest, const char *fname, error *e)
{
  FILE *fp = fopen (fname, "rw");
  if (fp == NULL)
    {
      panic ("TODO");
    }
  dest->fp = fp;
  return SUCCESS;
}

err_t
i_stream_open_r (i_stream *dest, const char *fname, error *e)
{
  FILE *fp = fopen (fname, "r");
  if (fp == NULL)
    {
      panic ("TODO");
    }
  dest->fp = fp;
  return SUCCESS;
}

err_t
i_stream_open_w (i_stream *dest, const char *fname, error *e)
{
  FILE *fp = fopen (fname, "w");
  if (fp == NULL)
    {
      panic ("TODO");
    }
  dest->fp = fp;
  return SUCCESS;
}

err_t
i_stream_close (i_stream *fp, error *e)
{
  if (fclose (fp->fp))
    {
      panic ("TODO");
    }
  return SUCCESS;
}

err_t
i_stream_eof (i_stream *fp, error *e)
{
  return feof (fp->fp);
}

err_t
i_stream_fsync (i_stream *fp, error *e)
{
  if (fflush (fp->fp))
    {
      panic ("TODO");
    }
  return SUCCESS;
}

i64
i_stream_read_some (i_stream *fp, void *dest, u64 nbytes, error *e)
{
  return fread (dest, nbytes, 1, fp->fp);
}

i64
i_stream_read_all (i_stream *fp, void *dest, u64 nbytes, error *e)
{
  // TODO loop
  return fread (dest, nbytes, 1, fp->fp);
}

i64
i_stream_read_all_expect (i_stream *fp, void *dest, u64 nbytes, error *e)
{
  // TODO loop
  return fread (dest, nbytes, 1, fp->fp);
}

i64
i_stream_write_some (i_stream *fp, const void *src, u64 nbytes, error *e)
{
  return fwrite (src, nbytes, 1, fp->fp);
}

err_t
i_stream_write_all (i_stream *fp, const void *src, u64 nbytes, error *e)
{
  // TODO loop
  return fwrite (src, nbytes, 1, fp->fp);
}

////////////////////////////////////////////////////////////
// Runtime

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
