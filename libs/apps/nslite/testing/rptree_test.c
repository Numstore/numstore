#include "numstore/core/error.h"
#include "numstore/intf/os/file_system.h"
#include "numstore/test/testing_test.h"
#include <numstore/test/rptree_stepper.h>
#include <numstore/test/testing.h>

#ifndef NTEST
RANDOM_TEST (TT_UNIT, rptree_random, 1)
{
  error e = error_create ();
  test_err_t_wrap (i_remove_quiet ("test.db", &e), &e);
  test_err_t_wrap (i_remove_quiet ("test.wal", &e), &e);
  struct rptv_stepper stepper;
  test_err_t_wrap (rptv_stepper_open (&stepper, "test.db", "test.wal", 1234, &e), &e);

  TEST_AGENT (1, "rptree random")
  {
    test_err_t_wrap (rptv_stepper_execute (&stepper, &e), &e);
  }
}
#endif
