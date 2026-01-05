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
 *   Internal header defining read operation structures and functions for the
 *   R+ tree cursor, supporting sequential and strided read patterns.
 */

#include <numstore/core/cbuffer.h>

struct rptree_cursor;

struct rptc_read
{
  // Parameters
  struct cbuffer *dest; // nullable
  t_size bsize;
  b_size stride;

  u32 bnext;
  b_size total_bread;
  b_size max_bread;

  enum
  {
    DLREAD_ACTIVE,
    DLREAD_SKIPPING,
  } state;
};

err_t rptc_read_execute (struct rptree_cursor *r, error *e);

// SEEKED -> READ
void rptc_seeked_to_read (
    struct rptree_cursor *r,
    struct cbuffer *dest,
    b_size max_nread,
    t_size bsize,
    u32 stride);

// READ -> UNSEEKED
err_t rptc_read_to_unseeked (struct rptree_cursor *r, error *e);
