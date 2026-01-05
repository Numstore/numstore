#pragma once

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
 *   Command-line interface parsing utilities for test programs. Supports test
 *   filters, flag parsing, and help text generation.
 */

#include <numstore/intf/types.h>

#include <config.h>

/**
 * A List of string - key for flags to or with
 * Example:
 * {
 *    { "FOO", FOO },
 *    { "BAR", 2 },
 *    { "BIZ", (1 << 5) },
 *    { NULL, 0 } // NULL-terminated
 * };
 */
struct cli_flag
{
  const char *name;
  int value;
};

//////////////////////////////////////////////////////////
/// TEST CLI PARAMS

struct test_cli
{
  struct
  {
    char *filters[CLI_MAX_FILTERS];
    int flen;
  };

  int flags;         // Type of tests to run
  double time_unit;  // Time unit for random tests
  bool help_printed; // True if help was displayed
};

void test_print_help (const char *prog);

int test_parse_cli_params (
    char **argv, int argc,
    struct test_cli *params,
    const struct cli_flag *flag_map);
