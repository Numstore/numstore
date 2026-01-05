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
 *   Implements cli.h. Command-line interface argument parsing utilities.
 */

#include <numstore/core/cli.h>

#include <numstore/test/testing.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define streq(a, b) (strcmp (a, b) == 0)

void
test_print_help (const char *prog)
{
  fprintf (stderr, "Usage: %s [options] [filter1 filter2 ...]\n", prog);
  fprintf (stderr, "\nOptions:\n");
  fprintf (stderr, "  UNIT             Run unit tests (default)\n");
  fprintf (stderr, "  HEAVY            Run heavy/long-running tests\n");
  fprintf (stderr, "  PROFILE          Run profiling tests\n");
  fprintf (stderr, "  TODO             Run TODO test stubs\n");
  fprintf (stderr, "  --time-unit SEC  Set time unit for random tests (in seconds)\n");
  fprintf (stderr, "                   Examples: --time-unit 0.001 (1ms, default)\n");
  fprintf (stderr, "                             --time-unit 0.01  (10ms)\n");
  fprintf (stderr, "                             --time-unit 1.0   (1 second)\n");
  fprintf (stderr, "  --help           Show this help message\n");
  fprintf (stderr, "\nFilters:\n");
  fprintf (stderr, "  Multiple filters can be specified. Tests matching ANY filter will run.\n");
  fprintf (stderr, "  If no filters specified, all tests of the selected type run.\n");
  fprintf (stderr, "\nExamples:\n");
  fprintf (stderr, "  %s                              # Run all unit tests\n", prog);
  fprintf (stderr, "  %s HEAVY                        # Run all heavy tests\n", prog);
  fprintf (stderr, "  %s HEAVY random                 # Run heavy tests matching 'random'\n", prog);
  fprintf (stderr, "  %s HEAVY random threadpool      # Run heavy tests matching 'random' OR 'threadpool'\n", prog);
  fprintf (stderr, "  %s lock async closure           # Run unit tests matching 'lock', 'async', or 'closure'\n", prog);
  fprintf (stderr, "  %s HEAVY --time-unit 0.01       # Run heavy tests with 10ms time unit\n", prog);
  fprintf (stderr, "  %s HEAVY random --time-unit 0.1 # Run random tests with 100ms time unit\n", prog);
}

int
test_parse_cli_params (
    char **argv,
    int argc,
    struct test_cli *params,
    const struct cli_flag *flag_map)
{
  params->flen = 0;
  params->flags = 0;
  params->time_unit = -1.0;
  params->help_printed = false;

  for (int i = 1; i < argc; i++)
    {
      char *arg = argv[i];

      if (streq (arg, "--help") || streq (arg, "-h"))
        {
          test_print_help (argv[0]);
          params->help_printed = true;
          return 0;
        }

      else if (streq (arg, "--time-unit"))
        {
          if (i + 1 >= argc)
            {
              fprintf (stderr, "Error: --time-unit requires a value\n");
              test_print_help (argv[0]);
              return -1;
            }

          // Parse time unit
          {
            char *endptr;
            params->time_unit = strtod (argv[i + 1], &endptr);
            if (*endptr != '\0' || params->time_unit <= 0)
              {
                fprintf (stderr,
                         "Error: Invalid time unit '%s'. Must be a positive number.\n",
                         argv[i + 1]);
                return -1;
              }
          }

          i++;
        }

      else
        {
          // Check for matching flag
          bool matched = false;
          if (flag_map)
            {
              for (const struct cli_flag *entry = flag_map; entry->name != NULL; entry++)
                {
                  if (streq (arg, entry->name))
                    {
                      params->flags |= entry->value;
                      matched = true;
                      break;
                    }
                }
            }

          // Everything else is a filter
          if (!matched)
            {
              if (params->flen >= CLI_MAX_FILTERS)
                {
                  fprintf (stderr, "Error: Too many filters (max %d)\n", CLI_MAX_FILTERS);
                  return -1;
                }

              params->filters[params->flen++] = arg;
            }
        }
    }

  return 0;
}

#ifndef NTEST

static const struct cli_flag test_flags[] = {
  { "--verbose", 1 << 0 },
  { "--debug", 1 << 1 },
  { "--trace", 1 << 2 },
  { NULL, 0 }
};

TEST (TT_UNIT, parse_cli_basic)
{
  TEST_CASE ("parse basic filters")
  {
    char *argv[] = { "prog", "filter1", "filter2", "filter3" };
    int argc = 4;
    struct test_cli params = { 0 };

    int ret = test_parse_cli_params (argv, argc, &params, NULL);

    test_assert (ret == 0);
    test_assert (params.flen == 3);
    test_assert (streq (params.filters[0], "filter1"));
    test_assert (streq (params.filters[1], "filter2"));
    test_assert (streq (params.filters[2], "filter3"));
    test_assert (params.flags == 0);
    test_assert (params.time_unit == -1.0);
  }

  TEST_CASE ("parse flags with filters")
  {
    char *argv[] = { "prog", "--verbose", "filter1", "--debug", "filter2" };
    int argc = 5;
    struct test_cli params = { 0 };

    int ret = test_parse_cli_params (argv, argc, &params, test_flags);

    test_assert (ret == 0);
    test_assert (params.flen == 2);
    test_assert (streq (params.filters[0], "filter1"));
    test_assert (streq (params.filters[1], "filter2"));
    test_assert (params.flags == ((1 << 0) | (1 << 1))); // verbose | debug
    test_assert (params.time_unit == -1.0);
  }

  TEST_CASE ("parse time unit")
  {
    char *argv[] = { "prog", "--time-unit", "2.5", "filter1" };
    int argc = 4;
    struct test_cli params = { 0 };

    int ret = test_parse_cli_params (argv, argc, &params, NULL);

    test_assert (ret == 0);
    test_assert (params.flen == 1);
    test_assert (streq (params.filters[0], "filter1"));
    test_assert (params.time_unit == 2.5);
  }

  TEST_CASE ("help flag returns zero")
  {
    char *argv[] = { "prog", "--help" };
    int argc = 2;
    struct test_cli params = { 0 };

    int ret = test_parse_cli_params (argv, argc, &params, NULL);

    test_assert (ret == 0);
  }

  TEST_CASE ("help short flag returns zero")
  {
    char *argv[] = { "prog", "-h" };
    int argc = 2;
    struct test_cli params = { 0 };

    int ret = test_parse_cli_params (argv, argc, &params, NULL);

    test_assert (ret == 0);
  }
}

TEST (TT_UNIT, parse_cli)
{
  TEST_CASE ("time unit without value fails")
  {
    char *argv[] = { "prog", "--time-unit" };
    int argc = 2;
    struct test_cli params = { 0 };

    int ret = test_parse_cli_params (argv, argc, &params, NULL);

    test_assert (ret == -1);
  }

  TEST_CASE ("invalid time unit fails")
  {
    char *argv[] = { "prog", "--time-unit", "not_a_number" };
    int argc = 3;
    struct test_cli params = { 0 };

    int ret = test_parse_cli_params (argv, argc, &params, NULL);

    test_assert (ret == -1);
  }

  TEST_CASE ("negative time unit fails")
  {
    char *argv[] = { "prog", "--time-unit", "-1.0" };
    int argc = 3;
    struct test_cli params = { 0 };

    int ret = test_parse_cli_params (argv, argc, &params, NULL);

    test_assert (ret == -1);
  }

  TEST_CASE ("zero time unit fails")
  {
    char *argv[] = { "prog", "--time-unit", "0" };
    int argc = 3;
    struct test_cli params = { 0 };

    int ret = test_parse_cli_params (argv, argc, &params, NULL);

    test_assert (ret == -1);
  }

  TEST_CASE ("time unit with trailing chars fails")
  {
    char *argv[] = { "prog", "--time-unit", "1.5abc" };
    int argc = 3;
    struct test_cli params = { 0 };

    int ret = test_parse_cli_params (argv, argc, &params, NULL);

    test_assert (ret == -1);
  }

  TEST_CASE ("too many filters fails")
  {
    // Build argv with CLI_MAX_FILTERS + 1 filters
    char *argv[CLI_MAX_FILTERS + 2]; // prog + filters + NULL
    argv[0] = "prog";

    // Allocate filter strings
    static char filter_strings[CLI_MAX_FILTERS + 1][32];
    for (int i = 0; i <= CLI_MAX_FILTERS; i++)
      {
        snprintf (filter_strings[i], sizeof (filter_strings[i]), "filter%d", i);
        argv[i + 1] = filter_strings[i];
      }

    int argc = CLI_MAX_FILTERS + 2;
    struct test_cli params = { 0 };

    int ret = test_parse_cli_params (argv, argc, &params, NULL);

    test_assert (ret == -1);
  }

  TEST_CASE ("unknown flag becomes filter")
  {
    char *argv[] = { "prog", "--unknown-flag", "filter1" };
    int argc = 3;
    struct test_cli params = { 0 };

    int ret = test_parse_cli_params (argv, argc, &params, test_flags);

    test_assert (ret == 0);
    test_assert (params.flen == 2);
    test_assert (streq (params.filters[0], "--unknown-flag"));
    test_assert (streq (params.filters[1], "filter1"));
    test_assert (params.flags == 0);
  }

  TEST_CASE ("complex mixed arguments")
  {
    char *argv[] = {
      "prog",
      "--verbose",
      "filter1",
      "--time-unit",
      "0.5",
      "--debug",
      "filter2",
      "--trace",
      "filter3"
    };
    int argc = 9;
    struct test_cli params = { 0 };

    int ret = test_parse_cli_params (argv, argc, &params, test_flags);

    test_assert (ret == 0);
    test_assert (params.flen == 3);
    test_assert (streq (params.filters[0], "filter1"));
    test_assert (streq (params.filters[1], "filter2"));
    test_assert (streq (params.filters[2], "filter3"));
    test_assert (params.flags == ((1 << 0) | (1 << 1) | (1 << 2)));
    test_assert (params.time_unit == 0.5);
  }
}

#endif
