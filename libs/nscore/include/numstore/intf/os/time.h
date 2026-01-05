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

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>

////////////////////////////////////////////////////////////
// Timer Handle
typedef struct i_timer i_timer;

#if PLATFORM_WINDOWS
struct i_timer
{
  LARGE_INTEGER frequency;
  LARGE_INTEGER start;
};
#elif PLATFORM_MAC || PLATFORM_IOS
struct i_timer
{
  u64 start;
  u64 numer;
  u64 denom;
};
#elif PLATFORM_EMSCRIPTEN
struct i_timer
{
  f64 start;
};
#else // POSIX (Linux, Android, BSD)
struct i_timer
{
  struct timespec start;
};
#endif

err_t i_timer_create (i_timer *timer, error *e);
void i_timer_free (i_timer *timer);
u64 i_timer_now_ns (i_timer *timer);
u64 i_timer_now_us (i_timer *timer);
u64 i_timer_now_ms (i_timer *timer);
f64 i_timer_now_s (i_timer *timer);

// Legacy API (deprecated - use i_timer instead)
void i_get_monotonic_time (struct timespec *ts);
