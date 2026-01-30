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
 *   Defines an in-memory R+ tree implementation for testing, providing read, write,
 *   insert, and remove operations on a simple byte array with locking support.
 */

// numstore
#include "numstore/core/slab_alloc.h"
#include <numstore/core/dbl_buffer.h>
#include <numstore/core/error.h>
#include <numstore/core/latch.h>
#include <numstore/intf/types.h>
#include <numstore/rptree/attr.h>

struct rptree_segment
{
  struct latch latch;
  u8 *view;
  u32 vcap;
  u32 vlen;
};

#define VTYPE struct rptree_segment *
#define KTYPE pgno
#define SUFFIX rptm
#include <numstore/core/robin_hood_ht.h>
#undef VTYPE
#undef KTYPE
#undef SUFFIX

struct rptree_mem
{
  struct slab_alloc alloc;
  struct latch l;

  hentry_rptm _hdata[256];
  hash_table_rptm pgno_to_segment;
};

////////////////////////////////////////////////////////////
/// MANAGEMENT

struct rptree_mem *rptm_open (error *e);
void rptm_close (struct rptree_mem *r);

////////////////////////////////////////////////////////////
/// ONE OFF

struct rptm_stride
{
  b_size bstart;
  u32 stride;
  b_size nelems;
};

err_t rptm_new (struct rptree_mem *n, pgno pg, error *e);
void rptm_delete (struct rptree_mem *n, pgno start);
b_size rptm_size (struct rptree_mem *n, pgno id);

err_t rptm_insert (
    struct rptree_mem *n,
    pgno id,
    const void *src,
    b_size bofst,
    t_size size,
    b_size nelem,
    error *e);

b_size rptm_write (
    struct rptree_mem *n,
    pgno id,
    const void *src,
    t_size size,
    struct rptm_stride stride);

b_size rptm_read (
    struct rptree_mem *n,
    pgno id,
    void *dest,
    t_size size,
    struct rptm_stride stride);

b_size rptm_remove (
    struct rptree_mem *n,
    pgno id,
    void *dest,
    t_size size,
    struct rptm_stride stride);
