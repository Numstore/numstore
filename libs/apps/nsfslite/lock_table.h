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
 *   Defines lock table data structures and interfaces for transaction concurrency control.
 *   Provides hierarchical locking for database resources (root page, variable hash pages,
 *   variables, and RPtree structures) with support for shared and exclusive lock modes.
 *   Maintains transaction-to-lock mappings using adaptive hash tables.
 */

#include <numstore/core/adptv_hash_table.h>
#include <numstore/core/clock_allocator.h>
#include <numstore/core/gr_lock.h>
#include <numstore/core/hash_table.h>
#include <numstore/core/spx_latch.h>
#include <numstore/pager/txn.h>

#include <config.h>

/**
 * A lt_lock is a handle on a gr_lock. There is a N -> 1 relationship between lt_lock and 
 * gr_locks. lt_locks are NOT to be copied. Their addresses are used by other locks. And 
 * are interacted with constantly
 *
 * The lock hierarchy goes:
 *
 * database:
 *   root page (page 0)
 *     first tombstone
 *     master lsn
 *   var_hash_page (page 1)
 *     hash_n 
 *   variable
 *     next
 *   rptree (pgno)
 */
struct lt_lock
{
  enum lt_lock_type
  {
    LOCK_DB,
    LOCK_ROOT,
    LOCK_FSTMBST,
    LOCK_MSLSN,
    LOCK_VHP,
    LOCK_VHPOS,
    LOCK_VAR,
    LOCK_VAR_NEXT,
    LOCK_RPTREE,
  } type;

  union lt_lock_data
  {
    p_size vhpos;
    pgno var_root;
    pgno var_root_next;
    pgno rptree_root;
  } data;

  // The gr lock reference (shared between the lock hash key)
  struct gr_lock *lock;
  enum lock_mode mode;

  // Node for the lock in the table
  struct hnode lock_type_node;
  struct hnode txn_node;

  txid parent_tid;
  struct lt_lock *next;
};

struct nsfsllt
{
  // Allocates locks for the lock table
  struct clck_alloc gr_lock_alloc;
  struct adptv_htable table;
  struct adptv_htable txn_index;
  struct spx_latch l;
};

err_t nsfslt_init (struct nsfsllt *t, error *e);
void nsfslt_destroy (struct nsfsllt *t);

err_t nsfslock (
    struct nsfsllt *t,
    struct lt_lock *lock,
    enum lt_lock_type type,
    union lt_lock_data data,
    enum lock_mode mode,
    struct txn *tx,
    error *e);

err_t nsfsunlock (struct nsfsllt *t, struct txn *tx, error *e);
