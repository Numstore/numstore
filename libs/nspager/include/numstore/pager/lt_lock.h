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

#include <numstore/core/gr_lock.h>
#include <numstore/core/hash_table.h>
#include <numstore/core/latch.h>

#include <config.h>

/**
 * A lt_lock is a handle on a gr_lock. There is a N -> 1 relationship between lt_lock and 
 * gr_locks. lt_locks are NOT to be copied. Their addresses are used by other locks. And 
 * are interacted with constantly
 *
 * The lock hierarchy goes:
 *
 * database: LOCK_DB
 *   root page (page 0) LOCK_ROOT
 *     first tombstone LOCK_FSTMBST
 *     master lsn LOCK_MLSN
 *   var_hash_page (page 1) LOCK_VHP
 *     hash_n LOCK_VHPOS
 *   variable (pgno) LOCK_VAR
 *     next LOCK_VAR_NEXT
 *   rptree (pgno) LOCK_RPTREE
 *   tmbst (pgno) LOCK_TMBST
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
    LOCK_TMBST,
  } type;

  union lt_lock_data
  {
    p_size vhpos;
    pgno var_root;
    pgno var_root_next;
    pgno rptree_root;
    pgno tmbst_pg;
  } data;

  struct gr_lock *lock;        // The actual lock (shared between the lock key)
  enum lock_mode mode;         // Lock mode locked under (S, X, SI, etc)
  struct hnode lock_type_node; // Node for the lock type in the table
  struct lt_lock *next;        // Next lock in this transaction id
  struct lt_lock *prev;        // Previous lock in this transaction id
  struct latch l;              // For thread safety
  txid tid;                    // Parent transaction id
};
