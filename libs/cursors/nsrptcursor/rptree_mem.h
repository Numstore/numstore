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
#include <numstore/core/error.h>
#include <numstore/core/latch.h>
#include <numstore/intf/types.h>
#include <numstore/rptree/attr.h>

struct rptree_mem
{
  struct latch latch;
  u8 *view;
  u32 vcap;
  u32 vlen;
};

////////////////////////////////////////////////////////////
/// MANAGEMENT

struct rptree_mem *rptm_new (error *e);
void rptm_close (struct rptree_mem *r);

////////////////////////////////////////////////////////////
/// ONE OFF

struct rptrd
{
  struct read_attr attr;
  t_size bsize;
  void *dest;
};

struct rptrm
{
  struct remove_attr attr;
  t_size bsize;
  void *dest;
};

struct rpti
{
  struct insert_attr attr;
  t_size bsize;
  void *src;
};

struct rptw
{
  struct write_attr attr;
  t_size bsize;
  void *src;
};

b_size rptm_read (struct rptree_mem *r, struct rptrd params);
b_size rptm_remove (struct rptree_mem *r, struct rptrm params);
err_t rptm_insert (struct rptree_mem *r, struct rpti params, error *e);
b_size rptm_write (struct rptree_mem *r, struct rptw params);
