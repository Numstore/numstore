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
 *   Implements latch.h. Atomic latch synchronization primitive.
 */

#include <numstore/core/latch.h>

#include <numstore/core/assert.h>
#include <numstore/intf/os.h>
#include <numstore/test/testing.h>

#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <unistd.h>

#define LOCKED 1
#define UNLOCKED 0

#ifndef NDEBUG
// Check if running under Valgrind
#ifdef RUNNING_ON_VALGRIND
#define DEADLOCK_THRESH 100000
#else
#define DEADLOCK_THRESH 1000000000
#endif
#define YIELD_INTERVAL 1000
#endif

void
latch_init (struct latch *latch)
{
  atomic_init (&latch->state, UNLOCKED);
}

void
latch_lock (struct latch *latch)
{
  uint_fast32_t expected;
#ifndef NDEBUG
  u32 nloops = 0;
#endif

  while (1)
    {
#ifndef NDEBUG
      if (nloops++ > DEADLOCK_THRESH)
        {
          ASSERTF (0, "Deadlock detected in latch lock!\n");
        }
      // Yield CPU periodically to avoid busy-wait under valgrind
      if (nloops % YIELD_INTERVAL == 0)
        {
          sched_yield ();
        }
#endif
      expected = UNLOCKED;

      // Try to acquire lock
      if (atomic_compare_exchange_weak (&latch->state, &expected, LOCKED))
        {
          return;
        }
    }
}

void
latch_unlock (struct latch *latch)
{
  atomic_store (&latch->state, UNLOCKED);
}

#ifndef NTEST
// Test context structure
struct latch_test_ctx
{
  struct latch *latch;
  int counter;
  int acquired;
};

static void
busy_wait_short (void)
{
  for (volatile int i = 0; i < 10000; ++i)
    {
    }
}

// Test basic lock acquire/release
TEST (TT_UNIT, latch_basic)
{
  struct latch latch;
  latch_init (&latch);

  latch_lock (&latch);
  test_assert_equal (atomic_load (&latch.state), LOCKED);

  latch_unlock (&latch);
  test_assert_equal (atomic_load (&latch.state), UNLOCKED);
}

// Test concurrent increments with lock
static void *
increment_thread (void *arg)
{
  struct latch_test_ctx *ctx = arg;
  for (int i = 0; i < 100; i++)
    {
      latch_lock (ctx->latch);
      int old = ctx->counter;
      busy_wait_short ();
      ctx->counter = old + 1;
      latch_unlock (ctx->latch);
    }
  return NULL;
}

TEST (TT_UNIT, latch_data_race_protection)
{
  struct latch latch;
  latch_init (&latch);

  struct latch_test_ctx ctx = { .latch = &latch };
  i_thread threads[5];
  error e = error_create ();

  for (int i = 0; i < 5; i++)
    {
      test_assert_equal (i_thread_create (&threads[i], increment_thread, &ctx, &e), SUCCESS);
    }

  for (int i = 0; i < 5; i++)
    {
      test_err_t_wrap (i_thread_join (&threads[i], &e), &e);
    }

  test_assert_equal (ctx.counter, 500);
  test_assert_equal (atomic_load (&latch.state), UNLOCKED);
}

// Helper thread
static void *
thread_hold_lock (void *arg)
{
  struct latch_test_ctx *ctx = arg;
  latch_lock (ctx->latch);
  ctx->acquired = 1;
  usleep (1000);
  latch_unlock (ctx->latch);
  return NULL;
}

static void *
thread_try_lock (void *arg)
{
  struct latch_test_ctx *ctx = arg;
  usleep (200);
  latch_lock (ctx->latch);
  ctx->acquired = 2;
  latch_unlock (ctx->latch);
  return NULL;
}

// Test mutual exclusion
TEST (TT_UNIT, latch_mutual_exclusion)
{
  struct latch latch;
  latch_init (&latch);
  struct latch_test_ctx ctx = { .latch = &latch };
  i_thread t1, t2;
  error e = error_create ();

  test_assert_equal (i_thread_create (&t1, thread_hold_lock, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_try_lock, &ctx, &e), SUCCESS);
  usleep (500);

  // Sample state while threads running
  int acquired_sample = ctx.acquired;

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);

  test_assert_int_equal (acquired_sample, 1); // Only first thread held lock at sample time
  test_assert_int_equal (ctx.acquired, 2);    // Second thread eventually acquired
  test_assert_equal (atomic_load (&latch.state), UNLOCKED);
}

#endif
