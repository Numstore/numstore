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
 *   Core string types (mutable and const) with length-prefixed representation
 *   and utility functions for converting from C-style null-terminated strings.
 */

// core
#include <numstore/intf/types.h>

struct string
{
  u32 len;
  char *data;
};

struct cstring
{
  u32 len;
  const char *data;
};

struct string strfcstr (char *cstr);
struct cstring cstrfcstr (const char *cstr);
