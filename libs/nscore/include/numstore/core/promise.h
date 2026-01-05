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
 *   Promise synchronization primitive for thread coordination, providing a
 *   simple wait/signal mechanism using mutexes and condition variables.
 */

#include <numstore/core/error.h>
#include <numstore/core/signatures.h>
#include <numstore/core/threadpool.h>
#include <numstore/intf/os.h>
#include <numstore/test/testing.h>

struct promise
{
  i_mutex mutex;
  i_cond ready;
  bool signaled;
};

HEADER_FUNC err_t
promise_create (struct promise *dest, error *e)
{
  if (i_mutex_create (&dest->mutex, e))
    {
      return e->cause_code;
    }

  if (i_cond_create (&dest->ready, e))
    {
      i_mutex_free (&dest->mutex);
      return e->cause_code;
    }

  return SUCCESS;
}

HEADER_FUNC void
promise_await (struct promise *p)
{
  i_mutex_lock (&p->mutex);
  i_cond_wait (&p->ready, &p->mutex);
  i_mutex_unlock (&p->mutex);

  // Release
  i_mutex_free (&p->mutex);
  i_cond_free (&p->ready);
}

HEADER_FUNC void
promise_signal (struct promise *p)
{
  i_mutex_lock (&p->mutex);
  p->signaled = true;
  i_cond_broadcast (&p->ready);
  i_mutex_unlock (&p->mutex);
}
