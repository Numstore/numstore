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
 *   Linear allocator (bump allocator) with state checkpoint and rollback support.
 *   Efficient for temporary allocations that can be reset in bulk.
 */

// core
#include <numstore/core/error.h>
#include <numstore/core/latch.h>
#include <numstore/intf/types.h>

struct lalloc
{
  struct latch latch;
  u32 used;
  u32 limit;
  u8 *data;
};

#define lalloc_create_from(buf, e) lalloc_create ((u8 *)buf, sizeof (buf), e)

struct lalloc lalloc_create (u8 *data, u32 limit, error *e);
u32 lalloc_get_state (const struct lalloc *l);
void lalloc_reset_to_state (struct lalloc *l, u32 state);
void *lmalloc (struct lalloc *a, u32 req, u32 size, error *e);
void *lcalloc (struct lalloc *a, u32 req, u32 size, error *e);
void lalloc_reset (struct lalloc *a);
