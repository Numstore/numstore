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
 *   Unified allocator interface that abstracts over multiple allocation strategies
 *   (linear, chunk-based, clock, and standard malloc).
 */

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/clock_allocator.h>
#include <numstore/core/lalloc.h>

struct alloc
{
  enum
  {
    AT_LALLOC,
    AT_CHNK_ALLOC,
    AT_CLK_ALLOC,
    AT_MALLOC,
  } type;

  union
  {
    struct chunk_alloc _calloc;
    struct lalloc _lalloc;
    struct clck_alloc _clk_alloc;
  };
};

void *alloc_alloc (struct alloc *a, u32 nelem, u32 size, error *e);
void *alloc_calloc (struct alloc *a, u32 nelem, u32 size, error *e);
void alloc_free (struct alloc *a, void *data);
