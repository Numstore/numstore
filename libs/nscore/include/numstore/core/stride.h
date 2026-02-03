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
 *   Public C API header for NumStore Lite (nslite). Provides a simplified
 *   interface for managing named variables in a transactional database with support for
 *   create, read, write, insert, remove operations using stride-based access patterns.
 *   Supports both implicit and explicit transaction management.
 */

#pragma once

#include <numstore/core/error.h>
#include <numstore/intf/types.h>

struct stride
{
  b_size start;
  u32 stride;
  b_size nelems;
};

#define STOP_PRESENT (1 << 0)
#define STEP_PRESENT (1 << 1)
#define START_PRESENT (1 << 2)

struct user_stride
{
  sb_size start;
  sb_size step;
  sb_size stop;

  int present; // bit mask for present -> 0000...00[START][STEP][STOP]
};

void stride_resolve_expect (struct stride *dest, struct user_stride src, b_size arrlen);
err_t stride_resolve (struct stride *dest, struct user_stride src, b_size arrlen, error *e);
