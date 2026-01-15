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
 *   Slab allocator
 */

#include "numstore/intf/os/memory.h"
#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/core/latch.h>
#include <numstore/intf/stdlib.h>
#include <numstore/intf/types.h>

struct slab;

struct slab_alloc
{
  struct slab *head;
  struct slab *current; // Cache slab with free space (hot path)
  struct latch l;
  u32 size;
  u32 cap_per_slab;
};

void slab_alloc_init (struct slab_alloc *dest, u32 size, u32 cap_per_slab);
void slab_alloc_destroy (struct slab_alloc *alloc);

void *slab_alloc_alloc (struct slab_alloc *alloc, error *e);
void slab_alloc_free (struct slab_alloc *alloc, void *ptr);
