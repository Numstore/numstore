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
 *   Implements spx_latch.h. Lock-free shared/exclusive latch operations.
 */

#include <numstore/core/spx_latch.h>

#include <numstore/core/assert.h>
#include <numstore/intf/os.h>
#include <numstore/test/testing.h>

#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <unistd.h>

#define PENDING_BIT (1ULL << 31)
#define COUNT_MASK (~PENDING_BIT)

#define is_X_set(val) ((val & COUNT_MASK) == COUNT_MASK)
#define no_S_left(val) ((val & COUNT_MASK) == 0)
#define is_P_set(val) (val & PENDING_BIT)
#define X_lock (PENDING_BIT | COUNT_MASK)
#define P_lock(val) ((val) | PENDING_BIT)
#define S_lock(val) ((val) + 1)

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
spx_latch_init (struct spx_latch *latch)
{
  atomic_init (&latch->state, 0);
}

void
spx_latch_lock_s (struct spx_latch *latch)
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
          ASSERTF (0, "Deadlock detected in s lock!\n");
        }
      // Yield CPU periodically to avoid busy-wait under valgrind
      if (nloops % YIELD_INTERVAL == 0)
        {
          sched_yield ();
        }
#endif
      expected = atomic_load (&latch->state);

      // Block on P or X lock
      if (is_P_set (expected) || is_X_set (expected))
        {
          continue;
        }

      // Verify nothing changed
      if (atomic_compare_exchange_weak (&latch->state, &expected, S_lock (expected)))
        {
          return;
        }
    }
}

void
spx_latch_unlock_s (struct spx_latch *latch)
{
  atomic_fetch_sub (&latch->state, 1);
}

void
spx_latch_lock_x (struct spx_latch *latch)
{
  uint_fast32_t expected;
#ifndef NDEBUG
  u32 nploops = 0;
  u32 nxloops = 0;
#endif

  while (1)
    {
#ifndef NDEBUG
      if (nploops++ > DEADLOCK_THRESH)
        {
          ASSERTF (0, "Deadlock detected in p lock!\n");
        }
      // Yield CPU periodically to avoid busy-wait under valgrind
      if (nploops % YIELD_INTERVAL == 0)
        {
          sched_yield ();
        }
#endif
      expected = atomic_load (&latch->state);

      // Pending lock is already set - skip it
      if (is_P_set (expected))
        {
          continue;
        }

      // Verify nothing changed and set P Lock
      if (!atomic_compare_exchange_weak (&latch->state, &expected, P_lock (expected)))
        {
          continue;
        }

      // drain S locks
      while (1)
        {
#ifndef NDEBUG
          if (nxloops++ > DEADLOCK_THRESH)
            {
              ASSERTF (0, "Deadlock detected in p lock!\n");
            }
          // Yield CPU periodically to avoid busy-wait under valgrind
          if (nxloops % YIELD_INTERVAL == 0)
            {
              sched_yield ();
            }
#endif
          expected = atomic_load (&latch->state);

          ASSERT (is_P_set (expected));

          // If number of S locks == 0
          if (no_S_left (expected))
            {
              // Verify nothing changed and set X Lock
              if (atomic_compare_exchange_weak (&latch->state, &expected, X_lock))
                {
                  return;
                }
            }
        }
    }
}

void
spx_latch_unlock_x (struct spx_latch *latch)
{
  atomic_store (&latch->state, 0);
}

void
spx_latch_upgrade_s_x (struct spx_latch *latch)
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
          ASSERTF (0, "Deadlock detected in upgrade!\n");
        }
      if (nloops % YIELD_INTERVAL == 0)
        {
          sched_yield ();
        }
#endif
      expected = atomic_load (&latch->state);

      // Must have at least one S lock (ours)
      ASSERT ((expected & COUNT_MASK) >= 1);

      // Can't upgrade if pending is already set
      if (is_P_set (expected))
        {
          continue;
        }

      if (!atomic_compare_exchange_weak (&latch->state, &expected, P_lock (expected - 1)))
        {
          continue;
        }

      // Same as X lock
      while (1)
        {
#ifndef NDEBUG
          if (nloops++ > DEADLOCK_THRESH)
            {
              ASSERTF (0, "Deadlock detected in upgrade drain!\n");
            }
          if (nloops % YIELD_INTERVAL == 0)
            {
              sched_yield ();
            }
#endif
          expected = atomic_load (&latch->state);

          ASSERT (is_P_set (expected));

          if (no_S_left (expected))
            {
              // All S locks drained, acquire X
              if (atomic_compare_exchange_weak (&latch->state, &expected, X_lock))
                {
                  return;
                }
            }
        }
    }
}

void
spx_latch_downgrade_x_s (struct spx_latch *latch)
{
  // Atomically go from X to S(1)
  // X lock is: PENDING_BIT | COUNT_MASK
  // S lock is: 1
  atomic_store (&latch->state, 1);
}

#ifdef SPXTEST
#ifndef NTEST

// Test context structure
struct spx_latchest_ctx
{
  struct spx_latch *latch;
  int counter;
  int s_acquired;
  int x_pending;
  int x_acquired;
  int s_blocked;
  int s1_acquired;
  int s2_acquired;
  int s3_acquired;
  int x1_acquired;
  int x2_pending;
  int x2_acquired;
};

static void
busy_wait_short (void)
{
  for (volatile int i = 0; i < 10000; ++i)
    {
    }
}

// Test basic S lock acquire/release
TEST_disabled (TT_UNIT, spx_latch_basic_shared)
{
  struct spx_latch latch;
  spx_latch_init (&latch);

  spx_latch_lock_s (&latch);
  test_assert (atomic_load (&latch.state) > 0);
  test_assert (!(atomic_load (&latch.state) & PENDING_BIT));

  spx_latch_unlock_s (&latch);
  test_assert_equal (atomic_load (&latch.state), 0);
}

// Test basic X lock acquire/release
TEST_disabled (TT_UNIT, spx_latch_basic_exclusive)
{
  struct spx_latch latch;
  spx_latch_init (&latch);

  spx_latch_lock_x (&latch);
  test_assert (atomic_load (&latch.state) & PENDING_BIT);
  test_assert_equal (atomic_load (&latch.state) & COUNT_MASK, COUNT_MASK);

  spx_latch_unlock_x (&latch);
  test_assert_equal (atomic_load (&latch.state), 0);
}

// Test multiple S locks can be held
TEST_disabled (TT_UNIT, spx_latch_multiple_shared_one_thread)
{
  struct spx_latch latch;
  spx_latch_init (&latch);

  spx_latch_lock_s (&latch);
  spx_latch_lock_s (&latch);
  spx_latch_lock_s (&latch);

  uint32_t state = atomic_load (&latch.state);
  test_assert_equal (state & COUNT_MASK, 3);
  test_assert (!(state & PENDING_BIT));

  spx_latch_unlock_s (&latch);
  spx_latch_unlock_s (&latch);
  spx_latch_unlock_s (&latch);
  test_assert_equal (atomic_load (&latch.state), 0);
}

// Helper threads
static void *
thread_hold_shared (void *arg)
{
  struct spx_latchest_ctx *ctx = arg;
  spx_latch_lock_s (ctx->latch);
  ctx->s_acquired = 1;
  usleep (1000); // Hold for 100ms
  spx_latch_unlock_s (ctx->latch);
  return NULL;
}

static void *
thread_acquire_exclusive (void *arg)
{
  struct spx_latchest_ctx *ctx = arg;
  usleep (200);
  ctx->x_pending = 1;
  spx_latch_lock_x (ctx->latch);
  ctx->x_acquired = 1;
  spx_latch_unlock_x (ctx->latch);
  return NULL;
}

static void *
thread_try_shared_after_pending (void *arg)
{
  struct spx_latchest_ctx *ctx = arg;
  usleep (400);
  spx_latch_lock_s (ctx->latch);
  ctx->s_blocked = 1;
  spx_latch_unlock_s (ctx->latch);
  return NULL;
}

static void *
thread_hold_exclusive (void *arg)
{
  struct spx_latchest_ctx *ctx = arg;
  spx_latch_lock_x (ctx->latch);
  ctx->x_acquired = 1;
  usleep (1000); // Hold for 100ms
  spx_latch_unlock_x (ctx->latch);
  return NULL;
}

static void *
thread_acquire_shared_quick (void *arg)
{
  struct spx_latchest_ctx *ctx = arg;
  usleep (200);
  spx_latch_lock_s (ctx->latch);
  ctx->s_acquired = 1;
  spx_latch_unlock_s (ctx->latch);
  return NULL;
}

static void *
thread_multiple_shared_1 (void *arg)
{
  struct spx_latchest_ctx *ctx = arg;
  spx_latch_lock_s (ctx->latch);
  ctx->s1_acquired = 1;
  usleep (800);
  spx_latch_unlock_s (ctx->latch);
  return NULL;
}

static void *
thread_multiple_shared_2 (void *arg)
{
  struct spx_latchest_ctx *ctx = arg;
  usleep (100);
  spx_latch_lock_s (ctx->latch);
  ctx->s2_acquired = 1;
  usleep (600);
  spx_latch_unlock_s (ctx->latch);
  return NULL;
}

static void *
thread_multiple_shared_3 (void *arg)
{
  struct spx_latchest_ctx *ctx = arg;
  usleep (200);
  spx_latch_lock_s (ctx->latch);
  ctx->s3_acquired = 1;
  usleep (400);
  spx_latch_unlock_s (ctx->latch);
  return NULL;
}

static void *
thread_hold_x1 (void *arg)
{
  struct spx_latchest_ctx *ctx = arg;
  spx_latch_lock_x (ctx->latch);
  ctx->x1_acquired = 1;
  usleep (1000);
  spx_latch_unlock_x (ctx->latch);
  return NULL;
}

static void *
thread_try_x2 (void *arg)
{
  struct spx_latchest_ctx *ctx = arg;
  usleep (200);
  ctx->x2_pending = 1;
  spx_latch_lock_x (ctx->latch);
  ctx->x2_acquired = 1;
  spx_latch_unlock_x (ctx->latch);
  return NULL;
}

// Test that S and X are exclusive
TEST_disabled (TT_UNIT, spx_latch_s_x_exclusive)
{
  struct spx_latch latch;
  spx_latch_init (&latch);
  struct spx_latchest_ctx ctx = { .latch = &latch };
  i_thread t1, t2;
  error e = error_create ();

  /**
   * t1: S(l); Sa(l); s_acquired=1; ----holds S---- uS(l)
   * t2:         x_pending=1; X(l); P(l); Pa(l) --waits-- Xa(l); x_acquired=1; uX(l)
   *
   * 00: t1 starts, acquires S
   * 01:
   * 02: t2 starts (20ms delay), sets pending but blocked by S
   * 03:
   * 04:
   * 05: SAMPLE HERE - s_acquired=1, x_pending=1, x_acquired=0 ✓
   * 06:
   * 07:
   * 08:
   * 09:
   * 10: t1 releases S, t2 gets X
   */

  test_assert_equal (i_thread_create (&t1, thread_hold_shared, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_acquire_exclusive, &ctx, &e), SUCCESS);
  usleep (500);

  // Sample state while threads running
  int s_acquired_sample = ctx.s_acquired;
  int x_pending_sample = ctx.x_pending;
  int x_acquired_sample = ctx.x_acquired;

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);

  test_assert (s_acquired_sample);
  test_assert (x_pending_sample);
  test_assert (!x_acquired_sample);
  test_assert (ctx.s_acquired);
  test_assert (ctx.x_pending);
  test_assert (ctx.x_acquired);
  test_assert_equal (atomic_load (&latch.state), 0);
}

// Test pending flag blocks new S locks
TEST_disabled (TT_UNIT, spx_latch_pending_blocks_shared)
{
  struct spx_latch latch;
  spx_latch_init (&latch);
  struct spx_latchest_ctx ctx = { .latch = &latch };
  i_thread t1, t2, t3;
  error e = error_create ();

  /**
   * Timeline (each line = 10ms):
   *
   * t1: S(l); Sa(l); s_acquired=1; ----holds S---- uS(l)
   * t2:         x_pending=1; X(l); P(l); Pa(l) --waits-- Xa(l); x_acquired=1; uX(l)
   * t3:                     S(l) -------blocked-------- Sa(l); s_blocked=1; uS(l)
   *
   * 00: t1 starts, acquires S
   * 01:
   * 02: t2 starts (20ms delay), sets pending
   * 03:
   * 04: t3 starts (40ms delay), tries S but blocked by PENDING_BIT
   * 05:
   * 06: SAMPLE HERE - s_acquired=1, x_pending=1, s_blocked=0 ✓
   * 07:
   * 08:
   * 09:
   * 10: t1 releases S, t2 gets X
   * 11: t2 releases X, t3 gets S
   */

  test_assert_equal (i_thread_create (&t1, thread_hold_shared, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_acquire_exclusive, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t3, thread_try_shared_after_pending, &ctx, &e), SUCCESS);
  usleep (600);

  // Sample state
  int s_acquired_sample = ctx.s_acquired;
  int x_pending_sample = ctx.x_pending;
  int s_blocked_sample = ctx.s_blocked;

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);
  test_err_t_wrap (i_thread_join (&t3, &e), &e);

  test_assert_int_equal (s_acquired_sample, 1);
  test_assert_int_equal (x_pending_sample, 1);
  test_assert_int_equal (s_blocked_sample, 0);
  test_assert (ctx.s_acquired);
  test_assert (ctx.x_pending);
  test_assert (ctx.x_acquired);
  test_assert_equal (atomic_load (&latch.state), 0);
}

// Test: X lock blocks S lock
TEST_disabled (TT_UNIT, spx_latch_x_blocks_s)
{
  struct spx_latch latch;
  spx_latch_init (&latch);
  struct spx_latchest_ctx ctx = { .latch = &latch };
  i_thread t1, t2;
  error e = error_create ();

  /**
   * Timeline (each line = 10ms):
   *
   * t1: X(l); Xa(l); x_acquired=1; ----holds X---- uX(l)
   * t2:         S(l) -----------blocked----------- Sa(l); s_acquired=1; uS(l)
   *
   * 00: t1 starts, acquires X
   * 01:
   * 02: t2 starts (20ms delay), tries S but blocked by X
   * 03:
   * 04:
   * 05: SAMPLE HERE - x_acquired=1, s_acquired=0 ✓
   * 06:
   * 07:
   * 08:
   * 09:
   * 10: t1 releases X, t2 gets S
   */

  test_assert_equal (i_thread_create (&t1, thread_hold_exclusive, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_acquire_shared_quick, &ctx, &e), SUCCESS);

  usleep (500);

  // Sample state
  int x_acquired_sample = ctx.x_acquired;
  int s_acquired_sample = ctx.s_acquired;

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);

  test_assert_int_equal (x_acquired_sample, 1);
  test_assert_int_equal (s_acquired_sample, 0); // Still blocked at sample time
  test_assert (ctx.x_acquired);
  test_assert (ctx.s_acquired); // Eventually acquired after X released
  test_assert_equal (atomic_load (&latch.state), 0);
}

// Test: Multiple S locks can coexist
TEST_disabled (TT_UNIT, spx_latch_multiple_shared)
{
  struct spx_latch latch;
  spx_latch_init (&latch);
  struct spx_latchest_ctx ctx = { .latch = &latch };
  i_thread t1, t2, t3;
  error e = error_create ();

  /**
   * Timeline (each line = 10ms):
   *
   * t1: S(l); Sa(l); s1=1; -----holds S----- uS(l)
   * t2:    S(l); Sa(l); s2=1; --holds S-- uS(l)
   * t3:         S(l); Sa(l); s3=1; -S- uS(l)
   *
   * 00: t1 starts, acquires S
   * 01: t2 starts (10ms), acquires S (count=2)
   * 02: t3 starts (20ms), acquires S (count=3)
   * 03:
   * 04: SAMPLE HERE - all 3 should have S lock ✓
   * 05:
   * 06: t3 releases (count=2)
   * 07: t2 releases (count=1)
   * 08: t1 releases (count=0)
   */

  test_assert_equal (i_thread_create (&t1, thread_multiple_shared_1, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_multiple_shared_2, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t3, thread_multiple_shared_3, &ctx, &e), SUCCESS);

  usleep (400);

  // Sample state - all should have acquired
  int s1_sample = ctx.s1_acquired;
  int s2_sample = ctx.s2_acquired;
  int s3_sample = ctx.s3_acquired;
  uint32_t state_sample = atomic_load (&latch.state);

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);
  test_err_t_wrap (i_thread_join (&t3, &e), &e);

  test_assert_int_equal (s1_sample, 1);
  test_assert_int_equal (s2_sample, 1);
  test_assert_int_equal (s3_sample, 1);
  test_assert_int_equal (state_sample, 3);           // Count should be 3
  test_assert_equal (atomic_load (&latch.state), 0); // All released
}

// Test: X waits for multiple S locks to drain
TEST_disabled (TT_UNIT, spx_latch_x_waits_for_multiple_s)
{
  struct spx_latch latch;
  spx_latch_init (&latch);
  struct spx_latchest_ctx ctx = { .latch = &latch };
  i_thread t1, t2, t3, t4;
  error e = error_create ();

  /**
   * Timeline (each line = 10ms):
   *
   * t1: S(l); Sa(l); s1=1; -----holds S----- uS(l)
   * t2:    S(l); Sa(l); s2=1; --holds S-- uS(l)
   * t3:         S(l); Sa(l); s3=1; -S- uS(l)
   * t4:              x_pending=1; X(l); P(l); Pa(l) --waits-- Xa(l); x=1; uX(l)
   *
   * 00: t1 starts, acquires S
   * 01: t2 starts, acquires S (count=2)
   * 02: t3 starts, acquires S (count=3)
   * 03: t4 starts, sets pending but can't get X (count=3)
   * 04:
   * 05: SAMPLE HERE - 3 S locks held, X pending but not acquired ✓
   * 06: t3 releases (count=2), X still waiting
   * 07: t2 releases (count=1), X still waiting
   * 08: t1 releases (count=0), X finally acquires
   */

  test_assert_equal (i_thread_create (&t1, thread_multiple_shared_1, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_multiple_shared_2, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t3, thread_multiple_shared_3, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t4, thread_acquire_exclusive, &ctx, &e), SUCCESS);

  usleep (500);

  // Sample state
  int s1_sample = ctx.s1_acquired;
  int s2_sample = ctx.s2_acquired;
  int s3_sample = ctx.s3_acquired;
  int x_pending_sample = ctx.x_pending;
  int x_acquired_sample = ctx.x_acquired;

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);
  test_err_t_wrap (i_thread_join (&t3, &e), &e);
  test_err_t_wrap (i_thread_join (&t4, &e), &e);

  test_assert_int_equal (s1_sample, 1);
  test_assert_int_equal (s2_sample, 1);
  test_assert_int_equal (s3_sample, 1);
  test_assert_int_equal (x_pending_sample, 1);
  test_assert_int_equal (x_acquired_sample, 0); // X still waiting at sample time
  test_assert (ctx.x_acquired);                 // Eventually acquired
  test_assert_equal (atomic_load (&latch.state), 0);
}

// Test: Two X locks are mutually exclusive
TEST_disabled (TT_UNIT, spx_latch_x_x_exclusive)
{
  struct spx_latch latch;
  spx_latch_init (&latch);
  struct spx_latchest_ctx ctx = { .latch = &latch };
  i_thread t1, t2;
  error e = error_create ();

  /**
   * Timeline (each line = 10ms):
   *
   * t1: X(l); Xa(l); x1=1; ----holds X---- uX(l)
   * t2:         x2_pending=1; X(l); P(l); Pa(l) --waits-- Xa(l); x2=1; uX(l)
   *
   * 00: t1 starts, acquires X
   * 01:
   * 02: t2 starts (20ms), tries X but blocked
   * 03:
   * 04:
   * 05: SAMPLE HERE - x1=1, x2_pending=1, x2_acquired=0 ✓
   * 06:
   * 07:
   * 08:
   * 09:
   * 10: t1 releases X, t2 gets X
   */

  test_assert_equal (i_thread_create (&t1, thread_hold_x1, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_try_x2, &ctx, &e), SUCCESS);

  usleep (500);

  int x1_sample = ctx.x1_acquired;
  int x2_pending_sample = ctx.x2_pending;
  int x2_acquired_sample = ctx.x2_acquired;

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);

  test_assert_int_equal (x1_sample, 1);
  test_assert_int_equal (x2_pending_sample, 1);
  test_assert_int_equal (x2_acquired_sample, 0);
  test_assert (ctx.x2_acquired); // Eventually acquired
  test_assert_equal (atomic_load (&latch.state), 0);
}

// Test concurrent increments with X lock
static void *
increment_thread (void *arg)
{
  struct spx_latchest_ctx *ctx = arg;
  for (int i = 0; i < 100; i++)
    {
      spx_latch_lock_x (ctx->latch);
      int old = ctx->counter;
      busy_wait_short ();
      ctx->counter = old + 1;
      spx_latch_unlock_x (ctx->latch);
    }
  return NULL;
}

TEST_disabled (TT_UNIT, spx_latch_data_race_protection)
{
  struct spx_latch latch;
  spx_latch_init (&latch);

  struct spx_latchest_ctx ctx = { .latch = &latch };
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
  test_assert_equal (atomic_load (&latch.state), 0);
}

#endif
#endif
