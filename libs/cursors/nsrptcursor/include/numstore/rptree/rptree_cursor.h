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
 *   Main header for the R+ tree cursor, defining the cursor structure and core
 *   operations for managing stateful traversal and modification of the R+ tree.
 */

// numstore
#include <numstore/core/latch.h>
#include <numstore/pager.h>
#include <numstore/pager/inner_node.h>
#include <numstore/rptree/_insert.h>
#include <numstore/rptree/_read.h>
#include <numstore/rptree/_rebalance.h>
#include <numstore/rptree/_remove.h>
#include <numstore/rptree/_seek.h>
#include <numstore/rptree/_write.h>

struct rptree_cursor
{
  enum
  {
    RPTS_UNSEEKED,
    RPTS_SEEKING,
    RPTS_SEEKED,
    RPTS_DL_INSERTING,
    RPTS_DL_REMOVING,
    RPTS_IN_REBALANCING,
    RPTS_DL_READING,
    RPTS_DL_WRITING,
    RPTS_PERMISSIVE,
  } state;

  struct latch latch;
  struct lockt *lt;

  ////////////////////////////////////////////////////////////
  // /
  /// Common Meta Data and Structures
  struct pager *pager;        // Common pager
  pgno meta_root;             // Root page cached
  pgno root;                  // Root page
  struct txn *tx;             // Current transaction
  p_size lidx;                // Local position
  struct node_updates _nupd1; // First Node updates section
  struct node_updates _nupd2; // Second Node updates section
  page_h cur;                 // Current page for sharing between states
  b_size total_size;          // Total sized cached
  struct
  {
    struct seek_v stack[20];
    u32 sp;
  } stack_state;

  ////////////////////////////////////////////////////////////
  // /
  /// Sub States
  union
  {
    struct rptc_seek seeker;
    struct rptc_remove remover;
    struct rptc_insert inserter;
    struct rptc_read reader;
    struct rptc_write writer;
    struct rptc_rebalance rebalancer;
  };
};

DEFINE_DBG_ASSERT (
    struct rptree_cursor, rptc_unseeked, r,
    {
      ASSERT (r->cur.mode == PHM_NONE);
    })

DEFINE_DBG_ASSERT (
    struct rptree_cursor, rptc_seeked, r,
    {
      ASSERT (r->root != PGNO_NULL);
      ASSERT (r->cur.mode != PHM_NONE);
      ASSERT (page_h_type (&r->cur) == PG_DATA_LIST);
    })

// State Utils
err_t rptc_pop_all (struct rptree_cursor *r, error *e);
err_t rptc_load_new_root (struct rptree_cursor *r, error *e);
err_t rptc_update_meta (struct rptree_cursor *r, error *e);

err_t rptc_balance_and_release (
    struct three_in_pair *output,
    struct root_update *root,
    struct rptree_cursor *r,
    page_h *prev,
    page_h *next,
    error *e);

// Logging
void i_log_rptree_cursor (int log_level, struct rptree_cursor *r);

// Runtime
err_t rptc_open (struct rptree_cursor *r, pgno root, struct pager *p, struct lockt *lt, error *e);
err_t rptc_new (struct rptree_cursor *r, struct txn *tx, struct pager *p, struct lockt *lt, error *e);
err_t rptc_cleanup (struct rptree_cursor *r, error *e);
err_t rptc_validate (struct rptree_cursor *r, error *e);

// Transactions
void rptc_enter_transaction (struct rptree_cursor *r, struct txn *tx);
void rptc_leave_transaction (struct rptree_cursor *r);
