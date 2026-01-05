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
 *   Implements promise.h. Tests for promise synchronization primitives.
 */

#include <numstore/core/promise.h>

#ifndef NTEST

struct ctx
{
  long data;
  struct promise *p;
};

static inline void
long_task (void *_data)
{
  struct ctx *ctx = _data;
  ctx->data = 0;

  for (long i = 0; i < 100000000; ++i)
    {
      ctx->data += i;
    }

  promise_signal (ctx->p);
}

TEST (TT_UNIT, promise)
{
  error e = error_create ();
  struct thread_pool *tp = tp_open (&e);
  test_fail_if_null (tp);

  test_err_t_wrap (tp_spin (tp, 2, &e), &e);

  struct promise p;
  test_err_t_wrap (promise_create (&p, &e), &e);

  struct ctx ctx = {
    .p = &p,
    .data = 0,
  };

  test_err_t_wrap (tp_add_task (tp, long_task, &ctx, &e), &e);

  promise_await (&p);

  test_assert_equal (ctx.data, 4999999950000000);

  test_err_t_wrap (tp_stop (tp, &e), &e);
  test_err_t_wrap (tp_free (tp, &e), &e);
}
#endif
