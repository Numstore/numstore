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
 *   Transaction table managing active transactions with locking and concurrency control mechanisms.
 */

#include <numstore/core/adptv_hash_table.h>
#include <numstore/core/alloc.h>
#include <numstore/core/bytes.h>
#include <numstore/core/clock_allocator.h>
#include <numstore/core/dbl_buffer.h>
#include <numstore/core/error.h>
#include <numstore/core/spx_latch.h>
#include <numstore/pager/txn.h>

struct txn_table
{
  struct adptv_htable t;
  struct spx_latch l;
};

// Lifecycle
err_t txnt_open (struct txn_table *dest, error *e);
void txnt_close (struct txn_table *t);

// Utils
void i_log_txnt (int log_level, struct txn_table *t);
err_t txnt_merge_into (struct txn_table *dest, struct txn_table *src, struct dbl_buffer *txn_dest, error *e);
slsn txnt_max_u_undo_lsn (struct txn_table *t);
void txnt_foreach (struct txn_table *t, void (*action) (struct txn *, void *ctx), void *ctx);
u32 txnt_get_size (struct txn_table *dest);

// Main Methods
bool txn_exists (struct txn_table *t, txid tid);

// INSERT
err_t txnt_insert_txn (struct txn_table *t, struct txn *tx, error *e);
err_t txnt_insert_txn_if_not_exists (struct txn_table *t, struct txn *tx, error *e);

// GET
bool txnt_get (struct txn **dest, struct txn_table *t, txid tid);
void txnt_get_expect (struct txn **dest, struct txn_table *t, txid tid);

// REMOVE
err_t txnt_remove_txn (bool *exists, struct txn_table *t, struct txn *tx, error *e);
err_t txnt_remove_txn_expect (struct txn_table *t, struct txn *unsafe_tx, error *e);

// SERIALIZATION
u32 txnt_get_serialize_size (struct txn_table *t);
u32 txnt_serialize (u8 *dest, u32 dlen, struct txn_table *t);
err_t txnt_deserialize (struct txn_table *dest, const u8 *src, struct txn *txn_bank, u32 slen, error *e);
u32 txnlen_from_serialized (u32 slen);

#ifndef NTEST
bool txnt_equal (struct txn_table *left, struct txn_table *right);
void txnt_rand_populate (struct txn_table *t, struct alloc *alloc);
void txnt_determ_populate (struct txn_table *t, struct alloc *alloc);
void txnt_crash (struct txn_table *t);
#endif
