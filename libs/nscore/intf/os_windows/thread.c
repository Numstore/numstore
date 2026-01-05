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
 *   Windows thread operations implementation
 */

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>

#include <windows.h>

// os
// system
#undef bool

static inline bool
handle_is_valid (HANDLE h)
{
  return h != INVALID_HANDLE_VALUE && h != NULL;
}

////////////////////////////////////////////////////////////
// Thread
err_t
i_thread_create (i_thread *t, void *(*start_routine) (void *), void *arg, error *e)
{
  HANDLE h = CreateThread (
      NULL,
      0,
      (LPTHREAD_START_ROUTINE)start_routine,
      arg,
      0,
      NULL);

  if (!handle_is_valid (h))
    {
      return error_causef (e, ERR_IO, "thread_create: Error %lu", GetLastError ());
    }

  t->handle = h;
  return SUCCESS;
}

err_t
i_thread_join (i_thread *t, error *e)
{
  if (WaitForSingleObject (t->handle, INFINITE) != WAIT_OBJECT_0)
    {
      return error_causef (e, ERR_IO, "thread_join: Error %lu", GetLastError ());
    }

  CloseHandle (t->handle);
  return SUCCESS;
}
