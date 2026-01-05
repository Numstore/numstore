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
 *   Darwin timer operations implementation
 */

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/intf/os.h>

#include <mach/mach_time.h>
#include <time.h>

////////////////////////////////////////////////////////////
// TIMING

// Timer API - handle-based monotonic timer
err_t
i_timer_create (i_timer *timer, error *e)
{
  ASSERT (timer);

  // Get timebase info for mach_absolute_time conversion
  mach_timebase_info_data_t timebase_info;
  if (mach_timebase_info (&timebase_info) != KERN_SUCCESS)
    {
      return error_causef (e, ERR_IO, "mach_timebase_info failed");
    }

  timer->numer = timebase_info.numer;
  timer->denom = timebase_info.denom;
  timer->start = mach_absolute_time ();

  return SUCCESS;
}

void
i_timer_free (i_timer *timer)
{
  ASSERT (timer);
  // No cleanup needed for Darwin timers
}

u64
i_timer_now_ns (i_timer *timer)
{
  ASSERT (timer);

  u64 now = mach_absolute_time ();
  u64 elapsed = now - timer->start;

  // Convert to nanoseconds
  return elapsed * timer->numer / timer->denom;
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
  // Use mach_absolute_time() for high-resolution timing on Darwin (macOS/iOS)
  static mach_timebase_info_data_t timebase_info;
  static int initialized = 0;

  if (!initialized)
    {
      mach_timebase_info (&timebase_info);
      initialized = 1;
    }

  uint64_t mach_time = mach_absolute_time ();

  // Convert to nanoseconds
  uint64_t nanoseconds = mach_time * timebase_info.numer / timebase_info.denom;

  // Convert to timespec (seconds and nanoseconds)
  ts->tv_sec = (long)(nanoseconds / 1000000000ULL);
  ts->tv_nsec = (long)(nanoseconds % 1000000000ULL);
}
