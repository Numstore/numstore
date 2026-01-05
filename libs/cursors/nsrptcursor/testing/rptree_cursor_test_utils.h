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
 *   Defines test utility structures and functions for creating synthetic R+ tree
 *   cursor states, facilitating isolated testing of cursor operations.
 */

// Core
#include <numstore/intf/types.h>
#include <numstore/pager/page_h.h>
#include <numstore/rptree/rptree_cursor.h>

// Private
struct rptc_rebalance_test_state
{
  b_size size;
  struct pager *p;
  struct txn *tx;
  p_size lidx;
  struct in_pair *left_input;
  struct in_pair *right_input;
  u32 lilen;
  u32 rilen;
  b_size pivot_key;

  page_h **stack;
  u32 stack_len;

  page_h *cur;
};

struct rptree_cursor rptc_from_rebalance_state (struct rptc_rebalance_test_state state);
