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
 *   Scratch/example program demonstrating thread pool usage.
 */

#include <numstore/core/threadpool.h>
#include <numstore/intf/os.h>
#include <numstore/intf/types.h>

#include "core/error.h"
#include "core/i_logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define t_diff(start, end) \
  ((end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9)

typedef struct
{
  int iters;
  u64 result;
} context;

static void
expensive_calculation (void *data)
{
  context *ctx = (context *)data;
  fprintf (stdout, "%d Starting\n", ctx->iters);
  int ret = 0;
  for (int i = 0; i < ctx->iters; ++i)
    {
      ret += i;
    }
  ctx->result = ret;
  fprintf (stdout, "%d Done\n", ctx->iters);
  free (ctx);
}

int
main (void)
{
  error e = error_create ();
  double time1, time2;

  // With Threading - keep spinning
  {
    struct thread_pool *w = tp_open (&e);
    tp_spin (w, get_available_threads (), &e);

    if (w == NULL)
      {
        fprintf (stderr, "Failed to create thread pool\n");
        return -1;
      }

    // Ignores system calls of thread set up
    struct timespec start, end;
    clock_gettime (CLOCK_MONOTONIC, &start);

    for (int i = 0; i < 1000; ++i)
      {
        i_log_info ("Adding task: %d\n", i);
        context *a = malloc (sizeof *a);
        if (a == NULL)
          {
            fprintf (stderr, "Failed to allocate\n");
          }
        a->iters = 10000000 + i;

        if (tp_add_task (w, expensive_calculation, a, &e))
          {
            fprintf (stderr, "Failed to add task\n");
            return -1;
          }
      }

    tp_stop (w, &e);

    clock_gettime (CLOCK_MONOTONIC, &end);
    time1 = t_diff (start, end);

    tp_free (w, &e);
  }

  // Without Threading
  {
    struct timespec start, end;
    clock_gettime (CLOCK_MONOTONIC, &start);

    for (int i = 0; i < 1000; ++i)
      {
        context *a = malloc (sizeof *a);
        if (a == NULL)
          {
            fprintf (stderr, "Failed to allocate\n");
          }
        a->iters = 10000000 + i;

        expensive_calculation (a);
      }

    clock_gettime (CLOCK_MONOTONIC, &end);
    time2 = t_diff (start, end);
  }

  fprintf (stdout, "Time Multi Threaded: %f seconds\n", time1);
  fprintf (stdout, "Time Single Threaded: %f seconds\n", time2);

  return 0;
}
