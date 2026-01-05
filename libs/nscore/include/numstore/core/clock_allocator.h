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
 *   TODO: Add description for clock_allocator.h
 */

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/core/latch.h>
#include <numstore/intf/stdlib.h>
#include <numstore/intf/types.h>

struct clck_alloc
{
  void *data;
  bool *occupied;
  u32 clock;
  size_t elem_size;
  u32 nelems;
  struct latch l;
};

err_t clck_alloc_open (struct clck_alloc *ca, size_t elem_size, u32 nelems, error *e);
void *clck_alloc_alloc (struct clck_alloc *ca, error *e);
void *clck_alloc_calloc (struct clck_alloc *ca, error *e);
void clck_alloc_free (struct clck_alloc *ca, void *ptr);
void clck_alloc_close (struct clck_alloc *ca);
