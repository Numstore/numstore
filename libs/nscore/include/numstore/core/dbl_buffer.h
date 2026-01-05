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
 *   Dynamic double buffer with automatic growth support. Thread-safe append
 *   operations for accumulating variable-sized data.
 */

// Core
#include <numstore/core/error.h>
#include <numstore/core/latch.h>
#include <numstore/intf/types.h>

struct dbl_buffer
{
  struct latch latch;
  void *data;
  u32 size;

  u32 nelem_cap;
  u32 nelem;
};

err_t dblb_create (struct dbl_buffer *dest, u32 size, u32 initial_cap, error *e);
err_t dblb_append (struct dbl_buffer *d, void *data, u32 nelem, error *e);
void *dblb_append_alloc (struct dbl_buffer *d, u32 nelem, error *e);
void dblb_free (struct dbl_buffer *d);
