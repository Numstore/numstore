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
 *   Windows timer operations implementation
 */

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/intf/os.h>
#include <numstore/intf/types.h>

#include <windows.h>

////////////////////////////////////////////////////////////
// TIMING

// Timer API - handle-based monotonic timer
err_t
i_timer_create (i_timer *timer, error *e)
{
  ASSERT (timer);

  if (!QueryPerformanceFrequency (&timer->frequency))
    {
      return error_causef (e, ERR_IO, "QueryPerformanceFrequency: Error %lu", GetLastError ());
    }

  if (!QueryPerformanceCounter (&timer->start))
    {
      return error_causef (e, ERR_IO, "QueryPerformanceCounter: Error %lu", GetLastError ());
    }

  return SUCCESS;
}

void
i_timer_free (i_timer *timer)
{
  ASSERT (timer);
  // No cleanup needed for Windows timers
}

u64
i_timer_now_ns (i_timer *timer)
{
  ASSERT (timer);

  LARGE_INTEGER now;
  QueryPerformanceCounter (&now);

  // Calculate elapsed ticks
  i64 elapsed = now.QuadPart - timer->start.QuadPart;

  // Convert to nanoseconds
  return (u64) ((elapsed * 1000000000LL) / timer->frequency.QuadPart);
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
  static LARGE_INTEGER frequency;
  static int initialized = 0;

  if (!initialized)
    {
      QueryPerformanceFrequency (&frequency);
      initialized = 1;
    }

  LARGE_INTEGER counter;
  QueryPerformanceCounter (&counter);

  // Convert to seconds and nanoseconds
  ts->tv_sec = (long)(counter.QuadPart / frequency.QuadPart);
  ts->tv_nsec = (long)(((counter.QuadPart % frequency.QuadPart) * 1000000000) / frequency.QuadPart);
}
