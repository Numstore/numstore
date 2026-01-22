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
 *   Dirty page table for tracking modified pages during transactions, supporting write-ahead logging and recovery operations.
 */

#include "numstore/core/adptv_hash_table.h"
#include <numstore/core/bytes.h>
#include <numstore/core/error.h>
#include <numstore/core/slab_alloc.h>
#include <numstore/intf/types.h>

#include <config.h>

struct dpg_table
{
  struct adptv_htable table; // Hash table pg -> entry
  struct slab_alloc alloc;   // Allocator for dpgt frames
  struct latch l;
};

// Lifecycle
err_t dpgt_open (struct dpg_table *dest, error *e);
void dpgt_close (struct dpg_table *t);

// Utils
void i_log_dpgt (int log_level, struct dpg_table *dpt);
err_t dpgt_merge_into (struct dpg_table *dest, struct dpg_table *src, error *e);
lsn dpgt_min_rec_lsn (struct dpg_table *d);
void dpgt_foreach (struct dpg_table *t, void (*action) (pgno pg, lsn rec_lsn, void *ctx), void *ctx);
u32 dpgt_get_size (struct dpg_table *d);

// Main Methods
bool dpgt_exists (struct dpg_table *t, pgno pg, error *e);

// INSERT
err_t dpgt_add (struct dpg_table *t, pgno pg, lsn rec_lsn, error *e);

// GET
bool dpgt_get (lsn *dest, struct dpg_table *t, pgno pg, error *e);
err_t dpgt_get_expect (lsn *dest, struct dpg_table *t, pgno pg, error *e);

// REMOVE
err_t dpgt_remove (bool *exists, struct dpg_table *t, pgno pg, error *e);
err_t dpgt_remove_expect (struct dpg_table *t, pgno pg, error *e);

// UPDATE
err_t dpgt_update (struct dpg_table *d, pgno pg, lsn new_rec_lsn, error *e);

// SERIALIZATION
u32 dpgt_get_serialize_size (struct dpg_table *t);
u32 dpgt_serialize (u8 *dest, u32 dlen, struct dpg_table *t);
err_t dpgt_deserialize (struct dpg_table *dest, const u8 *src, u32 slen, error *e);
u32 dpgtlen_from_serialized (u32 slen);

#ifndef NTEST
bool dpgt_equal (struct dpg_table *left, struct dpg_table *right, error *e);
err_t dpgt_rand_populate (struct dpg_table *t, error *e);
void dpgt_crash (struct dpg_table *t);
#endif
