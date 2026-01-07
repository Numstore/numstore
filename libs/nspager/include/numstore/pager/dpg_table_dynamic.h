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
 *   TODO: A dynamic dirty page table. See dirty page table
 */

#include <numstore/core/adptv_hash_table.h>
#include <numstore/core/error.h>
#include <numstore/core/latch.h>
#include <numstore/intf/types.h>

struct dpg_entry_dynamic
{
  pgno pg;           // Page number (key)
  lsn rec_lsn;       // Recovery LSN
  struct latch l;    // Entry-level latch
  struct hnode node; // Hash table node
};

struct dpg_table_dynamic
{
  struct adptv_htable t; // Underlying adaptive hash table
  struct latch l;        // Table-level latch
};

err_t dpgt_dyn_open (struct dpg_table_dynamic *dest, error *e);
err_t dpgt_dyn_close (struct dpg_table_dynamic *t, error *e);

err_t dpgt_dyn_add (struct dpg_table_dynamic *t, pgno pg, lsn rec_lsn, error *e);
err_t dpgt_dyn_add_or_update (struct dpg_table_dynamic *t, pgno pg, lsn rec_lsn, error *e);
bool dpgt_dyn_get (struct dpg_entry_dynamic *dest, struct dpg_table_dynamic *t, pgno pg);
void dpgt_dyn_get_expect (struct dpg_entry_dynamic *dest, struct dpg_table_dynamic *t, pgno pg);
bool dpgt_dyn_exists (struct dpg_table_dynamic *t, pgno pg);
err_t dpgt_dyn_remove (bool *exists, struct dpg_table_dynamic *t, pgno pg, error *e);
err_t dpgt_dyn_remove_expect (struct dpg_table_dynamic *t, pgno pg, error *e);
void dpgt_dyn_update (struct dpg_table_dynamic *t, pgno pg, lsn new_rec_lsn);

u32 dpgt_dyn_get_size (struct dpg_table_dynamic *t);
lsn dpgt_dyn_min_rec_lsn (struct dpg_table_dynamic *t);
void dpgt_dyn_foreach (struct dpg_table_dynamic *t, void (*action) (struct dpg_entry_dynamic *, void *ctx), void *ctx);

#ifndef NTEST
err_t dpgt_dyn_merge_into (struct dpg_table_dynamic *dest, struct dpg_table_dynamic *src, error *e);
bool dpgt_dyn_equals (struct dpg_table_dynamic *left, struct dpg_table_dynamic *right);
#endif

u32 dpgt_dyn_get_serialize_size (struct dpg_table_dynamic *t);

u32 dpgt_dyn_serialize (u8 *dest, u32 dlen, struct dpg_table_dynamic *t);

err_t dpgt_dyn_deserialize (struct dpg_table_dynamic *dest, const u8 *src, u32 slen, error *e);
