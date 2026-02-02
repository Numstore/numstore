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
 *   Main test runner executable that discovers and runs all registered NumStore tests with filtering and reporting.
 */

#include <numstore/core/cli.h>
#include <numstore/core/dbl_buffer.h>
#include <numstore/core/error.h>
#include <numstore/core/string.h>
#include <numstore/intf/logging.h>
#include <numstore/test/testing.h>

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static inline long
elapsed_ms (struct timespec s, struct timespec e)
{
  return (e.tv_sec - s.tv_sec) * 1000 + (e.tv_nsec - s.tv_nsec) / 1000000;
}

static inline bool
should_run (test *t, struct test_cli p)
{
  if (!(p.flags & t->tt))
    {
      return false;
    }

  if (p.flen == 0)
    {
      return true;
    }

  struct string tn = strfcstr (t->test_name);

  for (int i = 0; i < p.flen; i++)
    {
      struct string f = strfcstr (p.filters[i]);
      if (string_contains (tn, f))
        return true;
    }

  return false;
}

static const struct cli_flag test_flags[] = {
  { "UNIT", TT_UNIT },
  { "HEAVY", TT_HEAVY },
  { "PROFILE", TT_PROFILE },
  { NULL, 0 }
};

int
main (int argc, char **argv)
{
  struct test_cli p;
  if (test_parse_cli_params (argv, argc, &p, test_flags))
    {
      return -1;
    }
  if (p.help_printed)
    {
      return 0;
    }
  if (p.flags == 0)
    {
      p.flags = TT_UNIT;
    }
  if (p.time_unit > 0)
    {
      randt_set_unit (p.time_unit);
    }

  error e = error_create ();
  struct timespec st, en;
  clock_gettime (CLOCK_MONOTONIC, &st);

  struct dbl_buffer f;
  if (dblb_create (&f, sizeof (char *), 1, &e))
    return -1;

  for (u64 i = 0; i < ntests; ++i)
    {
      if (!should_run (&tests[i], p))
        {
          continue;
        }
      test_func fn = tests[i].test;
      if (!fn ())
        {
          char **n = &tests[i].test_name;
          if (dblb_append (&f, n, 1, &e))
            {
              dblb_free (&f);
              return -1;
            }
        }
    }

  clock_gettime (CLOCK_MONOTONIC, &en);
  printf ("Time: %ld ms\n", elapsed_ms (st, en));

  char **fl = f.data;
  if (f.nelem > 0)
    {
      i_log_failure ("FAILED TESTS:\n");
      for (u32 i = 0; i < f.nelem; ++i)
        i_log_failure ("%s\n", fl[i]);
    }
  else
    {
      i_log_passed ("ALL TESTS PASSED\n");
    }

  dblb_free (&f);
  return test_test_ret;
}
