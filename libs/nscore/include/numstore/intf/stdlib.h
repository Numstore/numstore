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
 *   Wrapped standard library functions (memcpy, memset, strlen, etc.) with
 *   consistent naming convention and interface for numstore internal use.
 */

#include <numstore/core/signatures.h>
#include <numstore/intf/types.h>

#include <stdio.h>
#include <string.h>

// system
#define i_strrchr(s, c) strrchr (s, c)
#define i_memset(s, c, n) memset (s, c, n)
HEADER_FUNC u32
i_memcpy (void *dest, const void *src, u32 bytes)
{
  memcpy (dest, src, bytes);
  return bytes;
}
#define i_memmove(dest, src, bytes) memmove (dest, src, bytes)
#define i_strncmp(left, right, len) strncmp (left, right, len)
#define i_strlen(cstr) strlen (cstr)
#define i_memcmp(s1, s2, n) memcmp (s1, s2, n)
#define i_memchr(buf, c, len) memchr (buf, c, len)
#define i_snprintf(buf, len, ...) snprintf (buf, len, __VA_ARGS__)
#define i_vsnprintf(buf, len, ...) vsnprintf (buf, len, __VA_ARGS__)
