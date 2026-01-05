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
 *   TODO: Add description for wal_file.h
 */

#include <numstore/core/latch.h>
#include <numstore/intf/os.h>
#include <numstore/pager/dirty_page_table.h>
#include <numstore/pager/page.h>
#include <numstore/pager/txn_table.h>
#include <numstore/pager/wal_rec_hdr.h>
#include <numstore/pager/wal_stream.h>

struct wal_file
{
  // Swap ostreams so we can flush in a seperate thread
  struct wal_ostream *current_ostream;
  struct wal_istream istream;
  bool istream_open;
  const char *fname;

  struct latch l;
};

err_t walf_open (struct wal_file *dest, const char *fname, error *e);
err_t walf_reset (struct wal_file *dest, error *e);
err_t walf_close (struct wal_file *w, error *e);
err_t walf_write_mode (struct wal_file *w, error *e);

lsn walf_get_next_lsn (struct wal_file *w);

// WRITE
slsn walf_write (struct wal_file *w, const struct wal_rec_hdr_write *src, error *e);

// READ
err_t walf_pread (struct wal_rec_hdr_read *dest, struct wal_file *w, lsn ofst, error *e);
err_t walf_read (struct wal_rec_hdr_read *dest, lsn *rlsn, struct wal_file *w, error *e);

// FLUSH
err_t walf_flush_to (struct wal_file *w, lsn l, error *e);

#ifndef NTEST
err_t walf_crash (struct wal_file *w, error *e);
#endif
