#include "numstore/core/error.h"
#include <numstore/core/stride.h>
#include <numstore/test/testing.h>

void
stride_resolve_expect (struct stride *dest, struct user_stride src, b_size arrlen)
{
  sb_size step = (src.present & STEP_PRESENT) ? src.step : 1;

  ASSERT (step > 0);

  if (arrlen == 0)
    {
      dest->start = 0;
      dest->stride = (u32)step;
      dest->nelems = 0;
      return;
    }

  sb_size start, stop;

  if (src.present & START_PRESENT)
    {
      start = src.start;
      if (start < 0)
        {
          start += arrlen;
        }

      // Clamp [0, arrlen]
      if (start < 0)
        {
          start = 0;
        }
      if (start > (sb_size)arrlen)
        {
          start = arrlen;
        }
    }
  else
    {
      start = 0;
    }

  if (src.present & STOP_PRESENT)
    {
      stop = src.stop;
      if (stop < 0)
        {
          stop += arrlen;
        }

      // Clamp [0, arrlen]
      if (stop < 0)
        {
          stop = 0;
        }
      if (stop > (sb_size)arrlen)
        {
          stop = arrlen;
        }
    }
  else
    {
      stop = arrlen;
    }

  b_size nelems;
  if (stop <= start)
    {
      nelems = 0;
    }
  else
    {
      nelems = (stop - start + step - 1) / step;
    }

  // Populate destination
  dest->start = (b_size)start;
  dest->stride = (u32)step;
  dest->nelems = nelems;

  return;
}

err_t
stride_resolve (struct stride *dest, struct user_stride src, b_size arrlen, error *e)
{
  sb_size step = (src.present & STEP_PRESENT) ? src.step : 1;

  if (step <= 0)
    {
      return error_causef (e, ERR_INVALID_ARGUMENT, "stride step must be positive");
    }

  stride_resolve_expect (dest, src, arrlen);

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, stride_resolve)
{
  struct stride result;
  error e;

  TEST_CASE ("Full slice [::] on length 10 ")
  {
    struct user_stride full = { 0 };
    test_err_t_wrap (stride_resolve (&result, full, 10, &e), &e);
    test_assert_int_equal (result.start, 0);
    test_assert_int_equal (result.stride, 1);
    test_assert_int_equal (result.nelems, 10);
  }

  TEST_CASE ("Slice with step [::2] on length 10 ")
  {
    struct user_stride step2 = { .step = 2, .present = STEP_PRESENT };
    test_err_t_wrap (stride_resolve (&result, step2, 10, &e), &e);
    test_assert_int_equal (result.start, 0);
    test_assert_int_equal (result.stride, 2);
    test_assert_int_equal (result.nelems, 5);
  }

  TEST_CASE ("Start only [5:] on length 10 ")
  {
    struct user_stride from5 = { .start = 5, .present = START_PRESENT };
    test_err_t_wrap (stride_resolve (&result, from5, 10, &e), &e);
    test_assert_int_equal (result.start, 5);
    test_assert_int_equal (result.stride, 1);
    test_assert_int_equal (result.nelems, 5);
  }

  TEST_CASE ("Stop only [:5] on length 10 ")
  {
    struct user_stride to5 = { .stop = 5, .present = STOP_PRESENT };
    test_err_t_wrap (stride_resolve (&result, to5, 10, &e), &e);
    test_assert_int_equal (result.start, 0);
    test_assert_int_equal (result.stride, 1);
    test_assert_int_equal (result.nelems, 5);
  }

  TEST_CASE ("Range [2:8] on length 10 ")
  {
    struct user_stride range = { .start = 2, .stop = 8, .present = START_PRESENT | STOP_PRESENT };
    test_err_t_wrap (stride_resolve (&result, range, 10, &e), &e);
    test_assert_int_equal (result.start, 2);
    test_assert_int_equal (result.stride, 1);
    test_assert_int_equal (result.nelems, 6);
  }

  TEST_CASE ("Range with step [1:9:2] on length 10 ")
  {
    struct user_stride range_step = { .start = 1, .stop = 9, .step = 2, .present = START_PRESENT | STOP_PRESENT | STEP_PRESENT };
    test_err_t_wrap (stride_resolve (&result, range_step, 10, &e), &e);
    test_assert_int_equal (result.start, 1);
    test_assert_int_equal (result.stride, 2);
    test_assert_int_equal (result.nelems, 4); // indices 1,3,5,7
  }

  TEST_CASE ("Negative start index [-3:] on length 10 -> [7:] ")
  {
    struct user_stride neg_start = { .start = -3, .present = START_PRESENT };
    test_err_t_wrap (stride_resolve (&result, neg_start, 10, &e), &e);
    test_assert_int_equal (result.start, 7);
    test_assert_int_equal (result.stride, 1);
    test_assert_int_equal (result.nelems, 3);
  }

  TEST_CASE ("Negative stop index [:-2] on length 10 -> [:8] ")
  {
    struct user_stride neg_stop = { .stop = -2, .present = STOP_PRESENT };
    test_err_t_wrap (stride_resolve (&result, neg_stop, 10, &e), &e);
    test_assert_int_equal (result.start, 0);
    test_assert_int_equal (result.stride, 1);
    test_assert_int_equal (result.nelems, 8);
  }

  TEST_CASE ("Both negative [-5:-2] on length 10 -> [5:8] ")
  {
    struct user_stride both_neg = { .start = -5, .stop = -2, .present = START_PRESENT | STOP_PRESENT };
    test_err_t_wrap (stride_resolve (&result, both_neg, 10, &e), &e);
    test_assert_int_equal (result.start, 5);
    test_assert_int_equal (result.stride, 1);
    test_assert_int_equal (result.nelems, 3);
  }

  TEST_CASE ("Out of bounds start [20:] on length 10 ")
  {
    struct user_stride oob_start = { .start = 20, .present = START_PRESENT };
    test_err_t_wrap (stride_resolve (&result, oob_start, 10, &e), &e);
    test_assert_int_equal (result.start, 10);
    test_assert_int_equal (result.stride, 1);
    test_assert_int_equal (result.nelems, 0);
  }

  TEST_CASE ("Empty slice [5:2] on length 10 ")
  {
    struct user_stride empty = { .start = 5, .stop = 2, .present = START_PRESENT | STOP_PRESENT };
    test_err_t_wrap (stride_resolve (&result, empty, 10, &e), &e);
    test_assert_int_equal (result.nelems, 0);
  }

  TEST_CASE ("Empty array ")
  {
    struct user_stride on_empty = { 0 };
    test_err_t_wrap (stride_resolve (&result, on_empty, 0, &e), &e);
    test_assert_int_equal (result.start, 0);
    test_assert_int_equal (result.stride, 1);
    test_assert_int_equal (result.nelems, 0);
  }

  TEST_CASE ("Invalid step = 0 ")
  {
    struct user_stride zero_step = { .step = 0, .present = STEP_PRESENT };
    test_assert_int_equal (stride_resolve (&result, zero_step, 10, &e), ERR_INVALID_ARGUMENT);
  }

  TEST_CASE ("Invalid negative step ")
  {
    struct user_stride neg_step = { .step = -1, .present = STEP_PRESENT };
    test_err_t_check (stride_resolve (&result, neg_step, 10, &e), ERR_INVALID_ARGUMENT, &e);
  }
}
#endif
