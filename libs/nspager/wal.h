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
 *   Internal write-ahead log implementation details and private structures.
 */

// numstore
#include <numstore/core/latch.h>
#include <numstore/pager/wal_file.h>

struct wal
{
  struct latch latch;
  struct wal_file wf;
  struct wal_rec_hdr_read rhdr;
  struct wal_rec_hdr_write whdr;
  struct thread_pool *tp;
};

// Lifecycle
err_t wal_open (struct wal *dest, const char *fname, error *e);
void wal_set_thread_pool (struct wal *w, struct thread_pool *tp);
err_t wal_reset (struct wal *dest, error *e);
err_t wal_close (struct wal *w, error *e);
err_t wal_write_mode (struct wal *w, error *e);

// FLUSH
err_t wal_flush_to (struct wal *w, lsn l, error *e);

// READ
struct wal_rec_hdr_read *wal_read_next (struct wal *w, lsn *read_lsn, error *e);
struct wal_rec_hdr_read *wal_read_entry (struct wal *w, lsn id, error *e);

// BEGIN
slsn wal_append_begin_log (struct wal *w, txid tid, error *e);

// COMMIT
slsn wal_append_commit_log (struct wal *w, txid tid, lsn prev, error *e);

// END
slsn wal_append_end_log (struct wal *w, txid tid, lsn prev, error *e);

// CHECKPOINT
slsn wal_append_ckpt_begin (struct wal *w, error *e);

// CHECKPOINT END
slsn wal_append_ckpt_end (struct wal *w, struct txn_table *att, struct dpg_table *dpt, error *e);

// UPDATE
slsn wal_append_update_log (struct wal *w, struct wal_update_write update, error *e);

// CLR
slsn wal_append_clr_log (struct wal *w, struct wal_clr_write clr, error *e);

// Arbitrary
slsn wal_append_log (struct wal *w, struct wal_rec_hdr_write *hdr, error *e);

#ifndef NTEST
err_t wal_crash (struct wal *w, error *e);
#endif
