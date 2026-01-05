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
 *   Windows mutex operations implementation
 */

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>

#include <windows.h>

////////////////////////////////////////////////////////////
// Mutex
err_t
i_mutex_create (i_mutex *m, error *e)
{
  InitializeCriticalSection (&m->cs);
  return SUCCESS;
}

void
i_mutex_free (i_mutex *m)
{
  DeleteCriticalSection (&m->cs);
}

void
i_mutex_lock (i_mutex *m)
{
  EnterCriticalSection (&m->cs);
}

void
i_mutex_unlock (i_mutex *m)
{
  LeaveCriticalSection (&m->cs);
}
