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
 *   Multi-granularity locking implementation with IS/IX/S/SIX/X modes
 */

#include "numstore/core/assert.h"
#include <numstore/core/gr_lock.h>

#include <numstore/intf/os.h>
#include <numstore/intf/stdlib.h>
#include <numstore/intf/types.h>
#include <numstore/test/testing.h>

#include <unistd.h>

static const bool compatible[LM_COUNT][LM_COUNT] = {
  //       IS     IX     S      SIX    X
  [LM_IS] = { true, true, true, true, false },
  [LM_IX] = { true, true, false, false, false },
  [LM_S] = { true, false, true, false, false },
  [LM_SIX] = { true, false, false, false, false },
  [LM_X] = { false, false, false, false, false },
};

static const char *mode_names[LM_COUNT] = { "IS", "IX", "S", "SIX", "X" };

err_t
gr_lock_init (struct gr_lock *l, error *e)
{
  err_t result = i_mutex_create (&l->mutex, e);
  if (result != SUCCESS)
    {
      return result;
    }

  i_memset (l->holder_counts, 0, sizeof (l->holder_counts));
  l->waiters = NULL;

  return SUCCESS;
}

void
gr_lock_destroy (struct gr_lock *l)
{
  i_mutex_free (&l->mutex);

  // Note: This assumes no threads are still waiting
  // TODO - Caller must ensure all threads have released locks

  while (l->waiters)
    {
      struct gr_lock_waiter *w = l->waiters;
      l->waiters = w->next;
      i_cond_free (&w->cond);
    }
}

static inline bool
is_compatible (struct gr_lock *l, enum lock_mode mode)
{
  for (int i = 0; i < LM_COUNT; i++)
    {
      if (l->holder_counts[i] > 0 && !compatible[mode][i])
        {
          return false;
        }
    }
  return true;
}

static inline void
wake_waiters (struct gr_lock *l)
{
  for (struct gr_lock_waiter *w = l->waiters; w; w = w->next)
    {
      if (is_compatible (l, w->mode))
        {
          i_cond_signal (&w->cond);
        }
    }
}

err_t
gr_lock (struct gr_lock *l, enum lock_mode mode, error *e)
{
  i_mutex_lock (&l->mutex);

  // Fast path: lock is available
  if (is_compatible (l, mode))
    {
      l->holder_counts[mode]++;
      i_mutex_unlock (&l->mutex);
      return SUCCESS;
    }

  // Slow path: need to wait
  struct gr_lock_waiter waiter = {
    .mode = mode,
    .next = NULL,
  };

  if (i_cond_create (&waiter.cond, e))
    {
      i_mutex_unlock (&l->mutex);
      return e->cause_code;
    }

  // Add to waiters list
  waiter.next = l->waiters;
  l->waiters = &waiter;

  // Wait until compatible
  while (!is_compatible (l, mode))
    {
      i_cond_wait (&waiter.cond, &l->mutex);
    }

  // Remove from waiters list - TODO - this remove is ugly
  struct gr_lock_waiter **ptr = &l->waiters;
  while (*ptr)
    {
      if (*ptr == &waiter)
        {
          *ptr = waiter.next;
          break;
        }
      ptr = &(*ptr)->next;
    }

  // Now safe to free - we're no longer in the list
  i_cond_free (&waiter.cond);

  // Acquire the lock
  l->holder_counts[mode]++;
  i_mutex_unlock (&l->mutex);

  return SUCCESS;
}

bool
gr_trylock (struct gr_lock *l, enum lock_mode mode)
{
  ASSERT (l);

  if (!i_mutex_try_lock (&l->mutex))
    {
      return false;
    }

  if (!is_compatible (l, mode))
    {
      i_mutex_unlock (&l->mutex);
      return false;
    }

  l->holder_counts[mode]++;
  i_mutex_unlock (&l->mutex);

  return true;
}

bool
gr_unlock (struct gr_lock *l, enum lock_mode mode)
{
  i_mutex_lock (&l->mutex);

  bool is_last = false;

  if (l->holder_counts[mode] > 0)
    {
      l->holder_counts[mode]--;

      // Check if this was the last holder
      bool any_holders = false;
      for (int i = 0; i < LM_COUNT; i++)
        {
          if (l->holder_counts[i] > 0)
            {
              any_holders = true;
              break;
            }
        }

      is_last = !any_holders && (l->waiters == NULL);

      // Wake any compatible waiters
      if (l->waiters)
        {
          wake_waiters (l);
        }
    }

  i_mutex_unlock (&l->mutex);

  return is_last;
}

err_t
gr_upgrade (struct gr_lock *l, enum lock_mode old_mode, enum lock_mode new_mode, error *e)
{
  ASSERT (l);
  ASSERT (new_mode > old_mode);

  i_mutex_lock (&l->mutex);

  ASSERT (l->holder_counts[old_mode] > 0);

  // Release old lock
  l->holder_counts[old_mode]--;

  // Fast path: can we acquire immediately?
  if (is_compatible (l, new_mode))
    {
      l->holder_counts[new_mode]++;
      i_mutex_unlock (&l->mutex);
      return SUCCESS;
    }

  // Slow path: need to wait
  struct gr_lock_waiter waiter = {
    .mode = new_mode,
    .next = NULL,
  };

  if (i_cond_create (&waiter.cond, e))
    {
      // restore old lock
      l->holder_counts[old_mode]++;
      i_mutex_unlock (&l->mutex);
      return e->cause_code;
    }

  // Add to waiters list
  waiter.next = l->waiters;
  l->waiters = &waiter;

  // Wait until compatible
  while (!is_compatible (l, new_mode))
    {
      i_cond_wait (&waiter.cond, &l->mutex);
    }

  // Remove from waiters list
  struct gr_lock_waiter **ptr = &l->waiters;
  while (*ptr)
    {
      if (*ptr == &waiter)
        {
          *ptr = waiter.next;
          break;
        }
      ptr = &(*ptr)->next;
    }

  // Now safe to free - we're no longer in the list
  i_cond_free (&waiter.cond);

  // Acquire the new lock
  l->holder_counts[new_mode]++;
  i_mutex_unlock (&l->mutex);

  return SUCCESS;
}

const char *
gr_lock_mode_name (enum lock_mode mode)
{
  if (mode >= 0 && mode < LM_COUNT)
    {
      return mode_names[mode];
    }
  return "INVALID";
}

#ifndef NTEST

struct lock_test_ctx
{
  struct gr_lock *lock;
  volatile int t1_acquired;
  volatile int t2_blocked;
  volatile int t2_acquired;
  volatile int counter;
  enum lock_mode mode1;
  enum lock_mode mode2;
};

static void
busy_wait_short (void)
{
  for (volatile int i = 0; i < 1000000; ++i)
    {
    }
}

// Test basic lock/unlock
TEST (TT_UNIT, gr_lock_basic)
{
  struct gr_lock lock;
  error e = error_create ();

  test_err_t_wrap (gr_lock_init (&lock, &e), &e);

  // Acquire and release each mode
  for (int mode = 0; mode < LM_COUNT; mode++)
    {
      test_err_t_wrap (gr_lock (&lock, mode, &e), &e);
      test_assert_equal (lock.holder_counts[mode], 1);
      gr_unlock (&lock, mode);
      test_assert_equal (lock.holder_counts[mode], 0);
    }

  gr_lock_destroy (&lock);
}

// Test multiple holders of same mode
TEST (TT_UNIT, gr_lock_multiple_holders)
{
  struct gr_lock lock;
  error e = error_create ();

  test_err_t_wrap (gr_lock_init (&lock, &e), &e);

  // S locks can be held multiple times
  test_err_t_wrap (gr_lock (&lock, LM_S, &e), &e);
  test_err_t_wrap (gr_lock (&lock, LM_S, &e), &e);
  test_err_t_wrap (gr_lock (&lock, LM_S, &e), &e);
  test_assert_equal (lock.holder_counts[LM_S], 3);

  gr_unlock (&lock, LM_S);
  test_assert_equal (lock.holder_counts[LM_S], 2);
  gr_unlock (&lock, LM_S);
  test_assert_equal (lock.holder_counts[LM_S], 1);
  gr_unlock (&lock, LM_S);
  test_assert_equal (lock.holder_counts[LM_S], 0);

  gr_lock_destroy (&lock);
}

// Helper thread functions for compatibility tests
static void *
thread_acquire_wait_release (void *arg)
{
  struct lock_test_ctx *ctx = arg;
  error e = error_create ();
  err_t result = gr_lock (ctx->lock, ctx->mode1, &e);
  if (result != SUCCESS)
    {
      return NULL;
    }
  ctx->t1_acquired = 1;
  usleep (100000); // Hold lock for 100ms
  gr_unlock (ctx->lock, ctx->mode1);
  return NULL;
}

static void *
thread_try_acquire (void *arg)
{
  struct lock_test_ctx *ctx = arg;
  error e = error_create ();
  usleep (20000); // Let t1 acquire first
  ctx->t2_blocked = 1;
  err_t result = gr_lock (ctx->lock, ctx->mode2, &e);
  if (result != SUCCESS)
    {
      return NULL;
    }
  ctx->t2_acquired = 1;
  ctx->t2_blocked = 0;
  gr_unlock (ctx->lock, ctx->mode2);
  return NULL;
}

// Test IS + IS (compatible)
TEST (TT_UNIT, gr_lock_is_is_compatible)
{
  struct gr_lock lock;
  error e = error_create ();

  test_err_t_wrap (gr_lock_init (&lock, &e), &e);

  struct lock_test_ctx ctx = { .lock = &lock, .mode1 = LM_IS, .mode2 = LM_IS };
  i_thread t1, t2;

  test_assert_equal (i_thread_create (&t1, thread_acquire_wait_release, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_try_acquire, &ctx, &e), SUCCESS);

  usleep (50000); // Check if both acquired

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);

  test_assert (ctx.t1_acquired);
  test_assert (ctx.t2_acquired);

  gr_lock_destroy (&lock);
}

// Test IS + IX (compatible)
TEST (TT_UNIT, gr_lock_is_ix_compatible)
{
  struct gr_lock lock;
  error e = error_create ();

  test_err_t_wrap (gr_lock_init (&lock, &e), &e);

  struct lock_test_ctx ctx = { .lock = &lock, .mode1 = LM_IS, .mode2 = LM_IX };
  i_thread t1, t2;
  test_assert_equal (i_thread_create (&t1, thread_acquire_wait_release, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_try_acquire, &ctx, &e), SUCCESS);

  usleep (50000);

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);

  test_assert (ctx.t1_acquired);
  test_assert (ctx.t2_acquired);

  gr_lock_destroy (&lock);
}

// Test IS + S (compatible)
TEST (TT_UNIT, gr_lock_is_s_compatible)
{
  struct gr_lock lock;
  error e = error_create ();

  test_err_t_wrap (gr_lock_init (&lock, &e), &e);

  struct lock_test_ctx ctx = { .lock = &lock, .mode1 = LM_IS, .mode2 = LM_S };
  i_thread t1, t2;
  test_assert_equal (i_thread_create (&t1, thread_acquire_wait_release, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_try_acquire, &ctx, &e), SUCCESS);

  usleep (50000);

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);

  test_assert (ctx.t1_acquired);
  test_assert (ctx.t2_acquired);

  gr_lock_destroy (&lock);
}

// Test IS + SIX (compatible)
TEST (TT_UNIT, gr_lock_is_six_compatible)
{
  struct gr_lock lock;
  error e = error_create ();

  test_err_t_wrap (gr_lock_init (&lock, &e), &e);

  struct lock_test_ctx ctx = { .lock = &lock, .mode1 = LM_IS, .mode2 = LM_SIX };
  i_thread t1, t2;
  test_assert_equal (i_thread_create (&t1, thread_acquire_wait_release, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_try_acquire, &ctx, &e), SUCCESS);

  usleep (50000);

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);

  test_assert (ctx.t1_acquired);
  test_assert (ctx.t2_acquired);

  gr_lock_destroy (&lock);
}

// Test IS + X (blocks)
TEST (TT_UNIT, gr_lock_is_x_blocks)
{
  struct gr_lock lock;
  error e = error_create ();

  test_err_t_wrap (gr_lock_init (&lock, &e), &e);

  struct lock_test_ctx ctx = { .lock = &lock, .mode1 = LM_IS, .mode2 = LM_X };
  i_thread t1, t2;
  test_assert_equal (i_thread_create (&t1, thread_acquire_wait_release, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_try_acquire, &ctx, &e), SUCCESS);

  usleep (50000);
  // Sample state while threads are still running
  int t1_acquired_sample = ctx.t1_acquired;
  int t2_blocked_sample = ctx.t2_blocked;
  int t2_acquired_sample = ctx.t2_acquired;

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);

  test_assert (t1_acquired_sample);
  test_assert (t2_blocked_sample);
  test_assert (!t2_acquired_sample);

  gr_lock_destroy (&lock);
}

// Test IX + IX (compatible)
TEST (TT_UNIT, gr_lock_ix_ix_compatible)
{
  struct gr_lock lock;
  error e = error_create ();

  test_err_t_wrap (gr_lock_init (&lock, &e), &e);

  struct lock_test_ctx ctx = { .lock = &lock, .mode1 = LM_IX, .mode2 = LM_IX };
  i_thread t1, t2;
  test_assert_equal (i_thread_create (&t1, thread_acquire_wait_release, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_try_acquire, &ctx, &e), SUCCESS);

  usleep (50000);

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);

  test_assert (ctx.t1_acquired);
  test_assert (ctx.t2_acquired);

  gr_lock_destroy (&lock);
}

// Test IX + S (blocks)
TEST (TT_UNIT, gr_lock_ix_s_blocks)
{
  struct gr_lock lock;
  error e = error_create ();

  test_err_t_wrap (gr_lock_init (&lock, &e), &e);

  struct lock_test_ctx ctx = { .lock = &lock, .mode1 = LM_IX, .mode2 = LM_S };
  i_thread t1, t2;
  test_assert_equal (i_thread_create (&t1, thread_acquire_wait_release, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_try_acquire, &ctx, &e), SUCCESS);

  usleep (50000);
  // Sample state while threads are still running
  int t1_acquired_sample = ctx.t1_acquired;
  int t2_blocked_sample = ctx.t2_blocked;

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);

  test_assert (t1_acquired_sample);
  test_assert (t2_blocked_sample);

  gr_lock_destroy (&lock);
}

// Test S + S (compatible)
TEST (TT_UNIT, gr_lock_s_compatible)
{
  struct gr_lock lock;
  error e = error_create ();

  test_err_t_wrap (gr_lock_init (&lock, &e), &e);

  struct lock_test_ctx ctx = { .lock = &lock, .mode1 = LM_S, .mode2 = LM_S };
  i_thread t1, t2;
  test_assert_equal (i_thread_create (&t1, thread_acquire_wait_release, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_try_acquire, &ctx, &e), SUCCESS);

  usleep (50000);

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);

  test_assert (ctx.t1_acquired);
  test_assert (ctx.t2_acquired);

  gr_lock_destroy (&lock);
}

// Test S + X (blocks)
TEST (TT_UNIT, gr_lock_x_blocks)
{
  struct gr_lock lock;
  error e = error_create ();

  test_err_t_wrap (gr_lock_init (&lock, &e), &e);

  struct lock_test_ctx ctx = { .lock = &lock, .mode1 = LM_S, .mode2 = LM_X };
  i_thread t1, t2;
  test_assert_equal (i_thread_create (&t1, thread_acquire_wait_release, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_try_acquire, &ctx, &e), SUCCESS);

  usleep (50000);
  // Sample state while threads are still running
  int t1_acquired_sample = ctx.t1_acquired;
  int t2_blocked_sample = ctx.t2_blocked;

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);

  test_assert (t1_acquired_sample);
  test_assert (t2_blocked_sample);

  gr_lock_destroy (&lock);
}

// Test SIX + IS (compatible - only compatible pair for SIX)
TEST (TT_UNIT, gr_lockix_is_compatible)
{
  struct gr_lock lock;
  error e = error_create ();

  test_err_t_wrap (gr_lock_init (&lock, &e), &e);

  struct lock_test_ctx ctx = { .lock = &lock, .mode1 = LM_SIX, .mode2 = LM_IS };
  i_thread t1, t2;
  test_assert_equal (i_thread_create (&t1, thread_acquire_wait_release, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_try_acquire, &ctx, &e), SUCCESS);

  usleep (50000);

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);

  test_assert (ctx.t1_acquired);
  test_assert (ctx.t2_acquired);

  gr_lock_destroy (&lock);
}

// Test SIX + IX (blocks)
TEST (TT_UNIT, gr_lockix_ix_blocks)
{
  struct gr_lock lock;
  error e = error_create ();

  test_err_t_wrap (gr_lock_init (&lock, &e), &e);

  struct lock_test_ctx ctx = { .lock = &lock, .mode1 = LM_SIX, .mode2 = LM_IX };
  i_thread t1, t2;
  test_assert_equal (i_thread_create (&t1, thread_acquire_wait_release, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_try_acquire, &ctx, &e), SUCCESS);

  usleep (50000);
  // Sample state while threads are still running
  int t1_acquired_sample = ctx.t1_acquired;
  int t2_blocked_sample = ctx.t2_blocked;

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);

  test_assert (t1_acquired_sample);
  test_assert (t2_blocked_sample);

  gr_lock_destroy (&lock);
}

// Test SIX + S (blocks)
TEST (TT_UNIT, gr_lockix_s_blocks)
{
  struct gr_lock lock;
  error e = error_create ();

  test_err_t_wrap (gr_lock_init (&lock, &e), &e);

  struct lock_test_ctx ctx = { .lock = &lock, .mode1 = LM_SIX, .mode2 = LM_S };
  i_thread t1, t2;
  test_assert_equal (i_thread_create (&t1, thread_acquire_wait_release, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_try_acquire, &ctx, &e), SUCCESS);

  usleep (50000);
  // Sample state while threads are still running
  int t1_acquired_sample = ctx.t1_acquired;
  int t2_blocked_sample = ctx.t2_blocked;

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);

  test_assert (t1_acquired_sample);
  test_assert (t2_blocked_sample);

  gr_lock_destroy (&lock);
}

// Test X + X (blocks)
TEST (TT_UNIT, gr_lock_sx_blocks)
{
  struct gr_lock lock;
  error e = error_create ();

  test_err_t_wrap (gr_lock_init (&lock, &e), &e);

  struct lock_test_ctx ctx = { .lock = &lock, .mode1 = LM_X, .mode2 = LM_X };
  i_thread t1, t2;
  test_assert_equal (i_thread_create (&t1, thread_acquire_wait_release, &ctx, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, thread_try_acquire, &ctx, &e), SUCCESS);

  usleep (50000);
  // Sample state while threads are still running
  int t1_acquired_sample = ctx.t1_acquired;
  int t2_blocked_sample = ctx.t2_blocked;

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);

  test_assert (t1_acquired_sample);
  test_assert (t2_blocked_sample);

  gr_lock_destroy (&lock);
}

// Test concurrent readers (multiple S locks)
static void *
reader_thread (void *arg)
{
  struct lock_test_ctx *ctx = arg;
  error e = error_create ();
  err_t result = gr_lock (ctx->lock, LM_S, &e);
  if (result != SUCCESS)
    {
      return NULL;
    }
  __sync_fetch_and_add (&ctx->counter, 1);
  busy_wait_short ();
  gr_unlock (ctx->lock, LM_S);
  return NULL;
}

TEST (TT_UNIT, gr_lock_concurrent_readers)
{
  struct gr_lock lock;
  error e = error_create ();

  test_err_t_wrap (gr_lock_init (&lock, &e), &e);

  struct lock_test_ctx ctx = { .lock = &lock };
  i_thread threads[5];
  for (int i = 0; i < 5; i++)
    {
      test_assert_equal (i_thread_create (&threads[i], reader_thread, &ctx, &e), SUCCESS);
    }

  for (int i = 0; i < 5; i++)
    {
      test_err_t_wrap (i_thread_join (&threads[i], &e), &e);
    }

  test_assert_equal (ctx.counter, 5); // All readers should acquire

  gr_lock_destroy (&lock);
}

// Test data race protection with exclusive locks
static void *
increment_thread (void *arg)
{
  struct lock_test_ctx *ctx = arg;
  error e = error_create ();
  for (int i = 0; i < 100; i++)
    {
      err_t result = gr_lock (ctx->lock, LM_X, &e);
      if (result != SUCCESS)
        {
          return NULL;
        }
      int old = ctx->counter;
      busy_wait_short ();
      ctx->counter = old + 1;
      gr_unlock (ctx->lock, LM_X);
    }
  return NULL;
}

TEST (TT_HEAVY, gr_lock_data_race_protection)
{
  struct gr_lock lock;
  error e = error_create ();

  test_err_t_wrap (gr_lock_init (&lock, &e), &e);

  struct lock_test_ctx ctx = { .lock = &lock };
  i_thread threads[5];
  for (int i = 0; i < 5; i++)
    {
      test_assert_equal (i_thread_create (&threads[i], increment_thread, &ctx, &e), SUCCESS);
    }

  for (int i = 0; i < 5; i++)
    {
      test_err_t_wrap (i_thread_join (&threads[i], &e), &e);
    }

  test_assert_equal (ctx.counter, 500); // 5 threads * 100 increments

  gr_lock_destroy (&lock);
}

// Test upgrade functionality
static void *
upgrade_thread (void *arg)
{
  struct lock_test_ctx *ctx = arg;
  error e = error_create ();

  // Acquire S lock
  err_t result = gr_lock (ctx->lock, LM_S, &e);
  if (result != SUCCESS)
    {
      return NULL;
    }

  ctx->t1_acquired = 1;
  usleep (20000);

  // Upgrade to X lock
  result = gr_upgrade (ctx->lock, LM_S, LM_X, &e);
  if (result != SUCCESS)
    {
      gr_unlock (ctx->lock, LM_S);
      return NULL;
    }

  ctx->t1_acquired = 2; // Mark as upgraded
  usleep (50000);

  gr_unlock (ctx->lock, LM_X);
  return NULL;
}

TEST (TT_UNIT, gr_lock_upgrade)
{
  struct gr_lock lock;
  error e = error_create ();

  test_err_t_wrap (gr_lock_init (&lock, &e), &e);

  struct lock_test_ctx ctx = { .lock = &lock };
  i_thread t1;

  test_assert_equal (i_thread_create (&t1, upgrade_thread, &ctx, &e), SUCCESS);

  usleep (100000);

  test_err_t_wrap (i_thread_join (&t1, &e), &e);

  test_assert_equal (ctx.t1_acquired, 2); // Verify upgrade succeeded

  gr_lock_destroy (&lock);
}

#endif
