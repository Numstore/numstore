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
 *   TODO: Add description for os.h
 */

#include <numstore/core/bytes.h>
#include <numstore/core/error.h>
#include <numstore/core/system.h>
#include <numstore/intf/os/file_system.h>
#include <numstore/intf/os/threading.h>
#include <numstore/intf/os/time.h>

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>

////////////////////////////////////////////////////////////
// Memory
void *i_malloc (u32 nelem, u32 size, error *e);
void *i_calloc (u32 nelem, u32 size, error *e);
void *i_realloc_right (void *ptr, u32 old_nelem, u32 nelem, u32 size, error *e);
void *i_realloc_left (void *ptr, u32 old_nelem, u32 nelem, u32 size, error *e);
void *i_crealloc_right (void *ptr, u32 old_nelem, u32 nelem, u32 size, error *e);
void *i_crealloc_left (void *ptr, u32 old_nelem, u32 nelem, u32 size, error *e);
void i_free (void *ptr);
#define i_cfree(ptr)    \
  do                    \
    {                   \
      if (ptr)          \
        {               \
          i_free (ptr); \
        }               \
    }                   \
  while (0)
