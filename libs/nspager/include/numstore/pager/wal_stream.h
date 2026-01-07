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
 *   Write-ahead log streaming interface for reading and writing log records with buffering.
 */

#include <numstore/core/cbuffer.h>
#include <numstore/core/latch.h>
#include <numstore/intf/os.h>

#include "config.h"

struct wal_ostream
{
  i_file fd;
  struct latch l;
  lsn flushed_lsn;

  struct cbuffer buffer;
  u8 _buffer[WAL_BUFFER_CAP];
};

// Lifecycle
struct wal_ostream *walos_open (const char *fname, error *e);
err_t walos_close (struct wal_ostream *w, error *e);

// Writing
err_t walos_flush_to (struct wal_ostream *w, lsn l, error *e);
err_t walos_write_all (struct wal_ostream *w, u32 *checksum, const void *data, u32 len, error *e);
lsn walos_get_next_lsn (struct wal_ostream *w);
err_t walos_truncate (struct wal_ostream *w, u64 howmuch, error *e);

struct wal_istream
{
  struct latch latch;
  i_file fd;
  lsn curlsn;
  lsn lsnidx;
};

// Lifecycle
err_t walis_open (struct wal_istream *dest, const char *fname, error *e);
err_t walis_close (struct wal_istream *w, error *e);

// Writing
err_t walis_seek (struct wal_istream *w, u64 pos, error *e);
void walis_mark_start_log (struct wal_istream *w);
err_t walis_read_all (struct wal_istream *w, bool *iseof, lsn *rlsn, u32 *checksum, void *data, u32 len, error *e);
void walis_mark_end_log (struct wal_istream *w);
err_t walis_truncate (struct wal_istream *w, u64 howmuch, error *e);

#ifndef NTEST
err_t walos_crash (struct wal_ostream *w, error *e);
err_t walis_crash (struct wal_istream *w, error *e);
#endif
