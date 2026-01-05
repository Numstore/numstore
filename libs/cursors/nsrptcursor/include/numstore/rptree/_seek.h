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
 *   Internal header defining seek operation structures and functions for navigating
 *   to specific byte offsets within the R+ tree structure.
 */

// numstore
#include <numstore/pager/page_h.h>

struct seek_v
{
  page_h pg;
  p_size lidx;
};

struct rptc_seek
{
  b_size remaining;
};

struct rptree_cursor;

// UTILS
err_t rptc_seek_pop_into_cur (struct rptree_cursor *r, error *e);

// SEEKING
err_t rptc_seeking_execute (struct rptree_cursor *r, error *e);

// UNSEEKED -> (SEEKING) -> SEEKED | UNSEEKED
err_t rptc_start_seek (
    struct rptree_cursor *r,
    b_size loc,
    bool newroot,
    error *e);
