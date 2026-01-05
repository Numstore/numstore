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
 *   Windows condition variable operations implementation
 */

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>

#include <windows.h>

////////////////////////////////////////////////////////////
// Condition Variable
err_t
i_cond_create (i_cond *c, error *e)
{
  InitializeConditionVariable (&c->cond);
  return SUCCESS;
}

void
i_cond_free (i_cond *c)
{
  // Condition variables don't need explicit cleanup
}

void
i_cond_wait (i_cond *c, i_mutex *m)
{
  SleepConditionVariableCS (&c->cond, &m->cs, INFINITE);
}

void
i_cond_signal (i_cond *c)
{
  WakeConditionVariable (&c->cond);
}

void
i_cond_broadcast (i_cond *c)
{
  WakeAllConditionVariable (&c->cond);
}
