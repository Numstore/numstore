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
 *   var_hash_page (page 1) LOCK_VHP
 *   variable (pgno) LOCK_VAR
 *   rptree (pgno) LOCK_RPTREE
 *   tmbst (pgno) LOCK_TMBST
 */
struct lt_lock
{
  enum lt_lock_type
  {
    LOCK_DB,
    LOCK_ROOT,
    LOCK_VHP,
    LOCK_VAR,
    LOCK_RPTREE,
    LOCK_TMBST,
  } type;

  union lt_lock_data
  {
    pgno var_root;
    pgno rptree_root;
    pgno tmbst_pg;
  } data;
};

u32 lt_lock_key (struct lt_lock lock);
bool lt_lock_equal (const struct lt_lock left, const struct lt_lock right);
void i_print_lt_lock (int log_level, struct lt_lock l);

bool get_parent (struct lt_lock *parent, struct lt_lock lock);
