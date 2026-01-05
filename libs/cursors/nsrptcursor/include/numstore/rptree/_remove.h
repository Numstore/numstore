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
 *   Internal header defining removal operation structures and functions for the
 *   R+ tree cursor, supporting element deletion with optional output buffering.
 */

#include <numstore/rptree/node_updates.h>

struct rptree_cursor;

struct rptc_remove
{
  struct cbuffer *dest; // nullable
  b_size total_removed;
  t_size bsize;
  b_size stride;
  b_size max_remove;

  // Output
  struct node_updates *output;
  page_h src;  // None on the first one (both cur)
  p_size sidx; // Source index (next)

  // Remove State
  b_size bnext;
  enum
  {
    DLREMOVE_REMOVING,
    DLREMOVE_SKIPPING,
  } state;
};

// SEEKED -> REMOVE
err_t rptc_seeked_to_remove (
    struct rptree_cursor *r,
    struct cbuffer *dest,
    b_size max_remove,
    t_size bsize,
    u32 stride,
    error *e);

// REMOVE
err_t rptc_remove_execute (struct rptree_cursor *r, error *e);

// REMOVE -> (REBALANCE) -> UNSEEKED
err_t rptc_remove_to_rebalancing_or_unseeked (struct rptree_cursor *r, error *e);
