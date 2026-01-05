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
 *   Implements statement.h. Provides statement memory allocation and
 *   deallocation functions for managing statement lifecycle.
 */

#include <numstore/compiler/statement.h>

#include <numstore/intf/os.h>

// core
struct statement *
statement_create (error *e)
{
  struct statement *ret = i_malloc (1, sizeof *ret, e);
  if (ret == NULL)
    {
      return NULL;
    }

  ret->qspace = lalloc_create_from (ret->query_space);
  return ret;
}

void
statement_free (struct statement *s)
{
  i_free (s);
}
