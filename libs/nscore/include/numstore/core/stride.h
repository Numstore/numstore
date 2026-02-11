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
#include <numstore/core/signatures.h>
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

#define USER_STRIDE_ALL ((struct user_stride){ .start = 0, .step = 1, .stop = -1, .present = ~0 })

bool ustride_equal (struct user_stride left, struct user_stride right);
void stride_resolve_expect (struct stride *dest, struct user_stride src, b_size arrlen);
err_t stride_resolve (struct stride *dest, struct user_stride src, b_size arrlen, error *e);

////////////////////////////////////////
/// Small Constructors

HEADER_FUNC struct user_stride
ustride012 (sb_size start, sb_size step, sb_size stop)
{
  return (struct user_stride){
    .start = start,
    .step = step,
    .stop = stop,
    .present = STOP_PRESENT | STEP_PRESENT | START_PRESENT,
  };
}

HEADER_FUNC struct user_stride
ustride01 (sb_size start, sb_size step)
{
  return (struct user_stride){
    .start = start,
    .step = step,
    .present = STEP_PRESENT | START_PRESENT,
  };
}

HEADER_FUNC struct user_stride
ustride0 (sb_size start)
{
  return (struct user_stride){
    .start = start,
    .present = START_PRESENT,
  };
}

HEADER_FUNC struct user_stride
ustride12 (sb_size step, sb_size stop)
{
  return (struct user_stride){
    .step = step,
    .stop = stop,
    .present = STOP_PRESENT | STEP_PRESENT,
  };
}

HEADER_FUNC struct user_stride
ustride1 (sb_size step)
{
  return (struct user_stride){
    .step = step,
    .present = STEP_PRESENT | START_PRESENT,
  };
}

HEADER_FUNC struct user_stride
ustride2 (sb_size stop)
{
  return (struct user_stride){
    .stop = stop,
    .present = STOP_PRESENT,
  };
}

HEADER_FUNC struct user_stride
ustride (void)
{
  return (struct user_stride){
    .present = 0,
  };
}
