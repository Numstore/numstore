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
 *   Windows spinlock operations implementation
 */

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>

#include <windows.h>

////////////////////////////////////////////////////////////
// Spinlock (Using Critical Section on Windows)
err_t
i_spinlock_create (i_spinlock *m, error *e)
{
  InitializeCriticalSection (&m->lock);
  return SUCCESS;
}

void
i_spinlock_free (i_spinlock *m)
{
  DeleteCriticalSection (&m->lock);
}

void
i_spinlock_lock (i_spinlock *m)
{
  EnterCriticalSection (&m->lock);
}

void
i_spinlock_unlock (i_spinlock *m)
{
  LeaveCriticalSection (&m->lock);
}
