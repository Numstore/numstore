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
 *   Scratch/example program demonstrating timer and performance measurement utilities.
 */

// Example demonstrating the cross-platform timer interface

#include <numstore/core/error.h>
#include <numstore/core/system.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>

#include <stdio.h>

int
main (void)
{
  error e = error_create ();

  printf ("=== Timer Example ===\n");
  printf ("Platform: %s\n\n", platformstr ());

  // Create a timer
  i_timer timer;
  if (i_timer_create (&timer, &e) != SUCCESS)
    {
      i_log_error ("Failed to create timer: %s\n", e.cause_msg);
      return 1;
    }

  i_log_info ("Timer created successfully\n");

  // Simulate some work
  volatile u64 sum = 0;
  for (u64 i = 0; i < 100000000; i++)
    {
      sum += i;
    }

  // Read timer values
  u64 ns = i_timer_now_ns (&timer);
  u64 us = i_timer_now_us (&timer);
  u64 ms = i_timer_now_ms (&timer);
  f64 s = i_timer_now_s (&timer);

  printf ("\nElapsed time:\n");
  printf ("  Nanoseconds:  %llu ns\n", (unsigned long long)ns);
  printf ("  Microseconds: %llu us\n", (unsigned long long)us);
  printf ("  Milliseconds: %llu ms\n", (unsigned long long)ms);
  printf ("  Seconds:      %.6f s\n", s);

  // Test multiple readings
  printf ("\nTesting timer precision:\n");
  for (int i = 0; i < 5; i++)
    {
      u64 t1 = i_timer_now_ns (&timer);
      u64 t2 = i_timer_now_ns (&timer);
      printf ("  Reading %d: %llu ns (overhead: %llu ns)\n",
              i + 1,
              (unsigned long long)t2,
              (unsigned long long)(t2 - t1));
    }

  // Cleanup
  i_timer_free (&timer);

  printf ("\n=== Timer Example Complete ===\n");
  return 0;
}
