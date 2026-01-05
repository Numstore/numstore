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
 *   Windows memory operations implementation
 */

#include <numstore/core/assert.h>
#include <numstore/core/bounds.h>
#include <numstore/core/error.h>
#include <numstore/intf/os.h>
#include <numstore/test/testing.h>

#include <stdlib.h>
#include <string.h>

////////////////////////////////////////////////////////////
// Memory
void *
i_malloc (u32 nelem, u32 size, error *e)
{
  void *ptr = malloc ((size_t)nelem * size);
  if (!ptr)
    {
      error_causef (e, ERR_NOMEM, "malloc failed");
    }
  return ptr;
}

void *
i_calloc (u32 nelem, u32 size, error *e)
{
  void *ptr = calloc (nelem, size);
  if (!ptr)
    {
      error_causef (e, ERR_NOMEM, "calloc failed");
    }
  return ptr;
}

void *
i_realloc_right (void *ptr, u32 old_nelem, u32 nelem, u32 size, error *e)
{
  void *new_ptr = realloc (ptr, (size_t)nelem * size);
  if (!new_ptr && nelem > 0)
    {
      error_causef (e, ERR_NOMEM, "realloc failed");
      return NULL;
    }
  return new_ptr;
}

void *
i_realloc_left (void *ptr, u32 old_nelem, u32 nelem, u32 size, error *e)
{
  // For Windows, we just do a regular realloc
  return i_realloc_right (ptr, old_nelem, nelem, size, e);
}

void *
i_crealloc_right (void *ptr, u32 old_nelem, u32 nelem, u32 size, error *e)
{
  void *new_ptr = i_realloc_right (ptr, old_nelem, nelem, size, e);
  if (new_ptr && nelem > old_nelem)
    {
      memset ((u8 *)new_ptr + old_nelem * size, 0, (nelem - old_nelem) * size);
    }
  return new_ptr;
}

void *
i_crealloc_left (void *ptr, u32 old_nelem, u32 nelem, u32 size, error *e)
{
  return i_crealloc_right (ptr, old_nelem, nelem, size, e);
}

void
i_free (void *ptr)
{
  free (ptr);
}
