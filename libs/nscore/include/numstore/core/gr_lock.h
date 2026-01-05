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
 *   Granular lock implementation supporting multiple lock modes (IS, IX, S, SIX, X)
 *   with compatibility matrix-based conflict resolution and waiter queues.
 */

#include <numstore/core/clock_allocator.h>
#include <numstore/core/error.h>
#include <numstore/intf/os.h>

enum lock_mode
{
  LM_IS = 0,
  LM_IX = 1,
  LM_S = 2,
  LM_SIX = 3,
  LM_X = 4,
  LM_COUNT = 5
};

struct gr_lock_waiter
{
  enum lock_mode mode;
  i_cond cond;
  struct gr_lock_waiter *next;
};

struct gr_lock
{
  i_mutex mutex;
  int holder_counts[LM_COUNT];
  struct gr_lock_waiter *waiters;
};

err_t gr_lock_init (struct gr_lock *l, error *e);
void gr_lock_destroy (struct gr_lock *l);

err_t gr_lock (struct gr_lock *l, enum lock_mode mode, error *e);
bool gr_trylock (struct gr_lock *l, enum lock_mode mode);
bool gr_unlock (struct gr_lock *l, enum lock_mode mode);
err_t gr_upgrade (struct gr_lock *l, enum lock_mode old_mode, enum lock_mode new_mode, error *e);
const char *gr_lock_mode_name (enum lock_mode mode);
