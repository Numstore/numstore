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
 *   Internal header defining rebalancing structures and functions for the R+ tree,
 *   managing node redistribution and tree structure maintenance after modifications.
 */

#include <numstore/rptree/node_updates.h>

struct root_update
{
  pgno root;
  bool isroot;
};

struct rptc_rebalance
{
  enum
  {
    RPTRB_RIGHT,
    RPTRB_LEFT,
  } state;

  struct node_updates *input;
  struct node_updates *output;

  page_h limit;
};

struct rptree_cursor;

// UTILS
err_t rptc_rebalance_execute_right (struct rptree_cursor *r, error *e);
err_t rptc_rebalance_execute_left (struct rptree_cursor *r, error *e);
err_t rptc_rebalance_right_to_left (struct rptree_cursor *r, error *e);
err_t rptc_rebalance_left_to_right (struct rptree_cursor *r, error *e);

err_t rptc_rebalance_move_up_stack (struct rptree_cursor *r, struct root_update root, error *e);

// REBALANCE
err_t rptc_rebalance_execute (struct rptree_cursor *r, error *e);
