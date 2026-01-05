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
 *   Lock-free shared/exclusive (reader-writer) latch using atomic operations.
 *   Provides S (shared) and X (exclusive) locking with upgrade/downgrade support.
 */

#include <numstore/intf/os.h>

struct spx_latch
{
  atomic_uint_fast32_t state; // bits: [31]=pending, [30:0]=count
};

void spx_latch_init (struct spx_latch *latch);

void spx_latch_lock_s (struct spx_latch *latch);
void spx_latch_lock_x (struct spx_latch *latch);

void spx_latch_upgrade_s_x (struct spx_latch *latch);
void spx_latch_downgrade_x_s (struct spx_latch *latch);

void spx_latch_unlock_s (struct spx_latch *latch);
void spx_latch_unlock_x (struct spx_latch *latch);
