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
 *   Implements closure.h. Function closures with context for callback patterns.
 */

#include <numstore/core/closure.h>

#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/test/testing.h>

#include <stdlib.h>
#include <time.h>

// core
// system
void
closure_execute (struct closure *cl)
{
  ASSERT (cl);
  ASSERT (cl->func);
  cl->func (cl->context);
}

#ifndef NTEST
typedef struct
{
  int *a;
  int *b;
  int *ret;
} add;

typedef struct
{
  int *a;
  int *b;
  int *ret;
} multiply;

static void
add_work (void *context)
{
  add *ccontext = context;
  test_fail_if_null (ccontext);
  *ccontext->ret = *ccontext->a + *ccontext->b;
}

static void
multiply_work (void *context)
{
  add *ccontext = context;
  test_fail_if_null (ccontext);
  *ccontext->ret = *ccontext->a * *ccontext->b;
}

static void
no_op_task (void *data)
{
  (void)data;
}

static void
set_flag_task (void *data)
{
  bool *flag = data;
  *flag = true;
}

TEST (TT_UNIT, closure)
{
  TEST_CASE ("basic_closure_chain")
  {
    int ret1;
    int ret2;
    int a = 5;
    int b = 7;
    int c = 9;

    struct closure add_closure = (struct closure){
      .func = add_work,
      .context = &(add){
          .a = &a,
          .b = &b,
          .ret = &ret1,
      }
    };

    struct closure mult_closure
        = (struct closure){
            .func = multiply_work,
            .context = &(multiply){
                .a = ((add *)(add_closure.context))->ret,
                .b = &c,
                .ret = &ret2,
            }
          };

    closure_execute (&add_closure);
    closure_execute (&mult_closure);

    test_assert_equal (ret1, (5 + 7));
    test_assert_equal (ret2, (5 + 7) * 9);
  }

  TEST_CASE ("no_context_closure")
  {
    struct closure no_op = (struct closure){
      .func = no_op_task,
      .context = NULL,
    };

    closure_execute (&no_op);
  }

  TEST_CASE ("multiple_closures_same_context")
  {
    bool flag = false;

    struct closure c1 = (struct closure){
      .func = set_flag_task,
      .context = &flag,
    };

    struct closure c2 = (struct closure){
      .func = set_flag_task,
      .context = &flag,
    };

    test_assert_equal (flag, false);
    closure_execute (&c1);
    test_assert_equal (flag, true);

    flag = false;
    closure_execute (&c2);
    test_assert_equal (flag, true);
  }

  TEST_CASE ("closure_array")
  {
    int values[5];
    add contexts[5];

    for (int i = 0; i < 5; ++i)
      {
        contexts[i] = (add){ .a = &values[i], .b = &values[i], .ret = &values[i] };
        values[i] = i + 1;
      }

    struct closure closures[5];
    for (int i = 0; i < 5; ++i)
      {
        closures[i] = (struct closure){ .func = add_work, .context = &contexts[i] };
      }

    // Execute all closures
    for (int i = 0; i < 5; ++i)
      {
        closure_execute (&closures[i]);
      }

    // Verify results
    for (int i = 0; i < 5; ++i)
      {
        test_assert_equal (values[i], 2 * (i + 1));
      }
  }
}
#endif
