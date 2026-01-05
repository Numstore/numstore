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
 *   Windows file operations implementation
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
