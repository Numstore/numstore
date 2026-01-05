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
 *   Internal header defining insertion operation structures and functions for
 *   the R+ tree cursor, including state management for ongoing insert operations.
 */

#include <numstore/rptree/node_updates.h>

struct rptree_cursor;

struct rptc_insert
{
  struct cbuffer *src;
  b_size total_written;
  b_size max_write;
  struct node_updates *output;
  pgno last; // Last page number to link at the end

  u8 temp_buf[DL_DATA_SIZE]; // Place to stash the right half of the starting page
  p_size tbw;                // Total written out from temp buf
  p_size tbl;                // Length of temp buf
};

/**
 * Saves all the right half of the current node to temp buf
 */
err_t rptc_seeked_to_insert (
    struct rptree_cursor *r,
    struct cbuffer *src,
    b_size max_write,
    error *e);

err_t rptc_insert_execute (struct rptree_cursor *r, error *e);

err_t rptc_insert_to_rebalancing_or_unseeked (struct rptree_cursor *r, error *e);
