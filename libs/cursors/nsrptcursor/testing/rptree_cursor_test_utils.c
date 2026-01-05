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
 *   Implements rptree_cursor_test_utils.h. Provides utility functions for
 *   constructing R+ tree cursor test states, enabling unit testing of
 *   rebalancing and other cursor operations.
 */

#include "rptree_cursor_test_utils.h"

struct rptree_cursor
rptc_from_rebalance_state (struct rptc_rebalance_test_state state)
{
  ASSERT (state.stack_len > 0);

  struct rptree_cursor cursor = {
    .state = RPTS_IN_REBALANCING,
    .total_size = state.size,
    .pager = state.p,
    .root = page_h_pgno (state.stack[0]),
    .tx = state.tx,
    // .cur init later
    .lidx = state.lidx,
    ._nupd1 = {
        // .left, .right init later
        .pivot = in_pair_from (in_get_leaf (page_h_ro (state.cur), state.lidx), state.pivot_key),
        .prev = NULL,
        .rlen = 1,
        .llen = 1,
        .robs = 0,
        .lobs = 0,
        .lcons = 0,
        .rcons = 0,
    },
    ._nupd2 = {
        .pivot = in_pair_from (page_h_pgno (state.cur), in_get_size (page_h_ro (state.cur))),
        .rlen = 0,
        .llen = 0,
        .robs = 0,
        .lobs = 0,
        .lcons = 0,
        .rcons = 0,
    },
    .cur = page_h_create (),
    .stack_state = {
        // .stack init later
        .sp = state.stack_len,
    },
    .rebalancer = {
        .state = -1,
        // .input, output init later
        .limit = page_h_create (),
    },
  };

  cursor.cur = page_h_xfer_ownership (state.cur);

  cursor.rebalancer.input = &cursor._nupd1;
  cursor.rebalancer.output = &cursor._nupd2;

  for (u32 i = 0; i < state.lilen; ++i)
    {
      cursor.rebalancer.input->left[i] = state.left_input[i];
    }
  for (u32 i = 0; i < state.rilen; ++i)
    {
      cursor.rebalancer.input->right[i] = state.right_input[i];
    }
  cursor.rebalancer.output->prev = &cursor.rebalancer.output->pivot;

  for (u32 i = 0; i < state.stack_len; ++i)
    {
      cursor.stack_state.stack[i].pg = page_h_xfer_ownership (state.stack[i]);
    }

  return cursor;
}
