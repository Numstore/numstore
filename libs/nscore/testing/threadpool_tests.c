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
 *   Integration tests for thread pool functionality.
 */

#include <numstore/core/assert.h>
#include <numstore/core/closure.h>
#include <numstore/core/error.h>
#include <numstore/core/threadpool.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/test/testing.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <windows.h>

#ifndef NTEST

// system
#ifndef _WIN32
#else
#define usleep(x) Sleep ((x) / 1000) // usleep takes microseconds, Sleep takes milliseconds
#endif

// To simulate cpu cycles
// For functional and demonstration of use, define
// three contexts - add, multiply and divide
struct add
{
  int a;
  int b;
  int ret;
  int large_calc;
  bool should_free;
};

struct multiply
{
  int a;
  int b;
  int ret;
  int large_calc;
  bool should_free;
};

struct divide
{
  float a;
  float b;
  float ret;
  int large_calc;
  bool should_free;
};

static int
expensive_calculation (int input)
{
  int ret = 0;
  for (int i = 0; i < input; ++i)
    {
      ret += i;
    }
  return ret;
}

static void
add_task (void *data)
{
  struct add *d = data;
  d->ret = d->a + d->b;
  d->large_calc = expensive_calculation (d->ret);
  if (d->should_free)
    {
      i_free (d);
    }
}

static void
multiply_task (void *data)
{
  struct multiply *d = data;
  d->ret = d->a * d->b;
  d->large_calc = expensive_calculation (d->ret);
  if (d->should_free)
    {
      i_free (d);
    }
}

static void
divide_task (void *data)
{
  struct divide *d = data;
  d->ret = d->a / d->b;
  d->large_calc = expensive_calculation (d->ret);
  if (d->should_free)
    {
      i_free (d);
    }
}

typedef union
{
  struct add a;
  struct multiply m;
  struct divide d;
} task;

static void
create_thread_pool_for_test (u32 num_tasks, u32 num_threads, double *out_time)
{
  error e = error_create ();

  struct thread_pool *w = tp_open (&e);
  task *tasks = i_malloc (num_tasks, sizeof *tasks, &e);
  int *types = i_malloc (num_tasks, sizeof *types, &e);

  test_fail_if_null (w);
  test_fail_if_null (tasks);
  test_fail_if_null (types);

  // Seed them
  for (u32 i = 0; i < num_tasks; ++i)
    {
      int arg1 = 1000000 + rand () % 1000000;
      int arg2 = 1000000 + rand () % 1000000;
      types[i] = (int)(rand () % 3);

      switch (types[i])
        {
        case 0:
          {
            tasks[i].a.a = arg1;
            tasks[i].a.b = arg2;
            tasks[i].a.should_free = false;
            break;
          }
        case 1:
          {
            tasks[i].m.a = arg1;
            tasks[i].m.b = arg2;
            tasks[i].m.should_free = false;
            break;
          }
        case 2:
          {
            tasks[i].d.a = (float)arg1;
            tasks[i].d.b = (float)arg2;
            tasks[i].d.should_free = false;
            break;
          }
        default:
          {
            UNREACHABLE ();
          }
        }
    }

  test_err_t_wrap (tp_spin (w, num_threads, &e), &e);

  struct timespec start, end;
  i_get_monotonic_time (&start);
  i_log_info ("Launching with %d threads\n", num_threads);

  // Run them
  for (u32 i = 0; i < num_tasks; ++i)
    {
      switch (types[i])
        {
        case 0:
          {
            test_assert_equal (tp_add_task (w, add_task, &tasks[i].a, &e), SUCCESS);
            break;
          }
        case 1:
          {
            test_assert_equal (tp_add_task (w, multiply_task, &tasks[i].m, &e), SUCCESS);
            break;
          }
        case 2:
          {
            test_assert_equal (tp_add_task (w, divide_task, &tasks[i].d, &e), SUCCESS);
            break;
          }
        default:
          {
            test_fail_if (1);
          }
        }
    }

  test_err_t_wrap (tp_stop (w, &e), &e);

  i_get_monotonic_time (&end);
  double ellapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

  test_err_t_wrap (tp_free (w, &e), &e);

  i_log_info ("Time(%d threads) = %f\n", num_threads, ellapsed);
  if (out_time)
    {
      *out_time = ellapsed;
    }

  // Verify Correctness
  for (u32 i = 0; i < num_tasks; ++i)
    {
      switch (types[i])
        {
        case 0:
          {
            test_assert_equal (tasks[i].a.a + tasks[i].a.b, tasks[i].a.ret);
            break;
          }
        case 1:
          {
            test_assert_equal (tasks[i].m.a * tasks[i].m.b, tasks[i].m.ret);
          }
          break;
        case 2:
          {
            test_assert_equal (tasks[i].d.a / tasks[i].d.b, tasks[i].d.ret);
            break;
          }
        default:
          {
            UNREACHABLE ();
          }
        }
    }
}

TEST (TT_UNIT, threadpool_basic)
{
  TEST_CASE ("create_and_destroy")
  {
    error e = error_create ();
    struct thread_pool *tp = tp_open (&e);
    test_fail_if_null (tp);
    test_assert (tp_not_spinning (tp));
    test_assert_equal (tp_free (tp, &e), SUCCESS);
  }

  TEST_CASE ("spin_and_stop")
  {
    error e = error_create ();
    struct thread_pool *tp = tp_open (&e);
    test_fail_if_null (tp);

    test_assert_equal (tp_spin (tp, 4, &e), SUCCESS);
    test_assert (tp_is_spinning (tp));

    test_assert_equal (tp_stop (tp, &e), SUCCESS);
    test_assert (tp_not_spinning (tp));

    test_assert_equal (tp_free (tp, &e), SUCCESS);
  }

  TEST_CASE ("add_tasks_before_spinning")
  {
    error e = error_create ();
    struct thread_pool *tp = tp_open (&e);
    test_fail_if_null (tp);

    int counter = 0;
    int num_tasks = 10;

    // Add tasks before spinning
    for (int i = 0; i < num_tasks; ++i)
      {
        struct add *data = i_malloc (1, sizeof *data, &e);
        data->a = i;
        data->b = i + 1;
        data->should_free = true;
        test_assert_equal (tp_add_task (tp, add_task, data, &e), SUCCESS);
      }

    // Now spin and execute
    test_assert_equal (tp_execute_until_done (tp, 4, &e), SUCCESS);

    test_assert_equal (tp_free (tp, &e), SUCCESS);
  }

  TEST_CASE ("multiple_spin_stop_cycles")
  {
    error e = error_create ();
    struct thread_pool *tp = tp_open (&e);
    test_fail_if_null (tp);

    for (int cycle = 0; cycle < 3; ++cycle)
      {
        test_assert_equal (tp_spin (tp, 2, &e), SUCCESS);
        test_assert (tp_is_spinning (tp));

        // Add a task
        struct add data = { .a = 1, .b = 2 };
        test_assert_equal (tp_add_task (tp, add_task, &data, &e), SUCCESS);

        // Give it time to execute
        usleep (100000);

        test_assert_equal (tp_stop (tp, &e), SUCCESS);
        test_assert (tp_not_spinning (tp));
      }

    test_assert_equal (tp_free (tp, &e), SUCCESS);
  }

  TEST_CASE ("empty_pool_execution")
  {
    error e = error_create ();
    struct thread_pool *tp = tp_open (&e);
    test_fail_if_null (tp);

    // Execute with no tasks
    test_assert_equal (tp_spin (tp, 2, &e), SUCCESS);
    test_assert_equal (tp_stop (tp, &e), SUCCESS);

    test_assert_equal (tp_free (tp, &e), SUCCESS);
  }

  TEST_CASE ("single_thread_pool")
  {
    error e = error_create ();
    struct thread_pool *tp = tp_open (&e);
    test_fail_if_null (tp);

    test_assert_equal (tp_spin (tp, 1, &e), SUCCESS);

    int num_tasks = 50;
    struct add *data = i_calloc (num_tasks, sizeof *data, &e);

    for (int i = 0; i < num_tasks; ++i)
      {
        data[i].a = i;
        data[i].b = i + 1;
        data[i].should_free = false;
        test_assert_equal (tp_add_task (tp, add_task, &data[i], &e), SUCCESS);
      }

    test_assert_equal (tp_stop (tp, &e), SUCCESS);

    // Verify all tasks completed
    for (int i = 0; i < num_tasks; ++i)
      {
        test_assert_equal (data[i].ret, i + (i + 1));
      }

    i_free (data);
    test_assert_equal (tp_free (tp, &e), SUCCESS);
  }
}

TEST (TT_HEAVY, threadpool)
{
  TEST_CASE ("Speed test")
  {
    error e = error_create ();

    double slow_time, fast_time;

    // SLOW
    create_thread_pool_for_test (100, 1, &slow_time);

    // FAST
    create_thread_pool_for_test (100, get_available_threads (), &fast_time);

    i_log_info ("Speedup: %.2fx\n", slow_time / fast_time);

    test_assert_equal (fast_time <= slow_time, true);
  }

  TEST_CASE ("high_concurrency_stress")
  {
    error e = error_create ();
    struct thread_pool *tp = tp_open (&e);
    test_fail_if_null (tp);

    test_assert_equal (tp_spin (tp, get_available_threads (), &e), SUCCESS);

    int num_tasks = 1000;
    struct add *data = i_calloc (num_tasks, sizeof *data, &e);

    for (int i = 0; i < num_tasks; ++i)
      {
        data[i].a = i;
        data[i].b = i * 2;
        data[i].should_free = false;
        test_assert_equal (tp_add_task (tp, add_task, &data[i], &e), SUCCESS);
      }

    test_assert_equal (tp_stop (tp, &e), SUCCESS);

    // Verify correctness
    for (int i = 0; i < num_tasks; ++i)
      {
        test_assert_equal (data[i].ret, i + i * 2);
      }

    i_free (data);
    test_assert_equal (tp_free (tp, &e), SUCCESS);
  }
}
#endif
