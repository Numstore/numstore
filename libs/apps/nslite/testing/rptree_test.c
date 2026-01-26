#include <numstore/test/rptree_stepper.h>
#include <numstore/test/testing.h>

#ifndef NTEST

RANDOM_TEST (TT_UNIT, rptv_stepper_random, 1)
{
  error e = error_create ();
  struct rptv_stepper *s = rptv_stepper_open ("test.db", "test.wal", 12345, &e);
  test_err_t_wrap (e.cause_code, &e);
  test_assert (s != NULL);
  test_assert (s->v != NULL);

  TEST_AGENT (1, "rptv stepper")
  {
    err_t result = rptv_stepper_execute (s, &e);
    test_err_t_wrap (result, &e);
  }

  err_t close_result = rptv_stepper_close (s, &e);
  test_err_t_wrap (close_result, &e);
}

#endif
