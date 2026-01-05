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
 *   Internal header defining write operation structures and functions for
 *   overwriting existing data in the R+ tree with strided write support.
 */

// system
#include <numstore/core/error.h>
#include <numstore/intf/types.h>

struct rptc_write
{
  // Parameters
  struct cbuffer *src;
  t_size bsize;
  b_size stride;

  // Cached
  b_size bstrlen; // stride * bsize

  u32 bnext;
  b_size total_bwritten;
  b_size max_bwrite;

  enum
  {
    DLWRITE_ACTIVE,
    DLWRITE_SKIPPING,
  } state;
};

struct rptree_cursor;

// WRITE
err_t rptc_write_execute (struct rptree_cursor *r, error *e);

// WRITE -> UNSEEKED
err_t rptc_write_to_unseeked (struct rptree_cursor *r, error *e);

// SEEKED -> WRITE
err_t rptc_seeked_to_write (
    struct rptree_cursor *r,
    struct cbuffer *src,
    b_size max_nwrite,
    t_size bsize,
    u32 stride,
    error *e);
