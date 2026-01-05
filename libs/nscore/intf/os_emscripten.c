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
 *   Implements os.h for Emscripten/WebAssembly platform-specific OS operations.
 */

// core
#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/intf/os.h>

#include <emscripten.h>

// os
// system
////////////////////////////////////////////////////////////
// TIMING

// Timer API - handle-based monotonic timer
err_t
i_timer_create (i_timer *timer, error *e)
{
  ASSERT (timer);

  // emscripten_get_now() returns milliseconds as double
  timer->start = emscripten_get_now ();

  return SUCCESS;
}

void
i_timer_free (i_timer *timer)
{
  ASSERT (timer);
  // No cleanup needed for Emscripten timers
}

u64
i_timer_now_ns (i_timer *timer)
{
  ASSERT (timer);

  f64 now = emscripten_get_now ();
  f64 elapsed_ms = now - timer->start;

  // Convert milliseconds to nanoseconds
  return (u64) (elapsed_ms * 1000000.0);
}

u64
i_timer_now_us (i_timer *timer)
{
  return i_timer_now_ns (timer) / 1000ULL;
}

u64
i_timer_now_ms (i_timer *timer)
{
  return i_timer_now_ns (timer) / 1000000ULL;
}

f64
i_timer_now_s (i_timer *timer)
{
  return (f64)i_timer_now_ns (timer) / 1000000000.0;
}

// Legacy API (deprecated - use i_timer instead)
void
i_get_monotonic_time (struct timespec *ts)
{
  f64 now_ms = emscripten_get_now ();
  f64 now_s = now_ms / 1000.0;

  ts->tv_sec = (long)now_s;
  ts->tv_nsec = (long)((now_s - (f64)ts->tv_sec) * 1000000000.0);
}
