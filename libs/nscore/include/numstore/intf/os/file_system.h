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
 *   TODO: Add description for os.h
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
// Stream Read / Write
i64 i_read_some (i_file *fp, void *dest, u64 nbytes, error *e);
i64 i_read_all (i_file *fp, void *dest, u64 nbytes, error *e);
i64 i_read_all_expect (i_file *fp, void *dest, u64 nbytes, error *e);
i64 i_write_some (i_file *fp, const void *src, u64 nbytes, error *e);
err_t i_write_all (i_file *fp, const void *src, u64 nbytes, error *e);

////////////////////////////////////////////////////////////
// Others
err_t i_truncate (i_file *fp, u64 bytes, error *e);
err_t i_fallocate (i_file *fp, u64 bytes, error *e);
i64 i_file_size (i_file *fp, error *e);
err_t i_remove_quiet (const char *fname, error *e);
err_t i_mkstemp (i_file *dest, char *tmpl, error *e);
err_t i_unlink (const char *name, error *e);
err_t i_mkdir (const char *name, error *e);

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
err_t i_dir_exists (const char *fname, bool *dest, error *e);
err_t i_file_exists (const char *fname, bool *dest, error *e);
