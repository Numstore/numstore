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
 *   Implements string.h. Core string type conversion utilities.
 */

#include <numstore/core/string.h>

#include <numstore/core/assert.h>
#include <numstore/intf/stdlib.h>
#include <numstore/test/testing.h>

// core
DEFINE_DBG_ASSERT (
    struct string, string, s,
    {
      ASSERT (s);
      ASSERT (s->data);
      ASSERT (s->len > 0);
    })

DEFINE_DBG_ASSERT (
    struct string, cstring, s,
    {
      DBG_ASSERT (string, s);
      ASSERT (s->data[s->len] == 0);
    })

struct string
strfcstr (char *cstr)
{
  return (struct string){
    .data = cstr,
    .len = i_strlen (cstr)
  };
}

struct cstring
cstrfcstr (const char *cstr)
{
  return (struct cstring){
    .data = cstr,
    .len = i_strlen (cstr)
  };
}
