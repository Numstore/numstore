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
 *   Lightweight spinlock synchronization primitive using atomic operations
 *   for protecting critical sections in concurrent code.
 */

#include "numstore/intf/os/threading.h"
#include <numstore/core/assert.h>
#include <numstore/core/signatures.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>

struct latch
{
  i_spinlock lock;
  int holder_uuid;
};

HEADER_FUNC err_t
latch_init (struct latch *latch, error *e)
{
  err_t ret = i_spinlock_create (&latch->lock, e);
  if (ret < SUCCESS)
    {
      return ret;
    }
  latch->holder_uuid = 0;
  return SUCCESS;
}

HEADER_FUNC void
latch_lock (struct latch *latch)
{
  i_spinlock_lock (&latch->lock);
}

HEADER_FUNC void
latch_unlock (struct latch *latch)
{
  latch->holder_uuid++;
  i_spinlock_unlock (&latch->lock);
}
