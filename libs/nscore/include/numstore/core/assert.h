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
 *   Core assertion and crash handling utilities. Conditionally includes debug or
 *   release assertion implementations based on NDEBUG flag.
 */

#include <numstore/intf/os.h>

#ifdef _MSC_VER
#define UNREACHABLE_HINT() __assume (0)
#else
#define UNREACHABLE_HINT() __builtin_unreachable ()
#endif

#define crash()               \
  do                          \
    {                         \
      i_print_stack_trace (); \
      *(volatile int *)0 = 1; \
      UNREACHABLE_HINT ();    \
    }                         \
  while (0)

#define UNIMPLEMENTED() UNREACHABLE ()

#define UNREACHABLE() \
  do                  \
    {                 \
      crash ();       \
    }                 \
  while (0)

#ifndef NDEBUG
#include "assert_debug.h"
#else
#include "assert_release.h"
#endif
