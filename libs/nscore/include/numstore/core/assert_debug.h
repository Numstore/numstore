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
 *   Debug-mode assertion macros with detailed error reporting, stack traces,
 *   and formatted assertion messages. Active only in debug builds.
 */

// core
#include <numstore/core/macros.h>
#include <numstore/core/signatures.h>
#include <numstore/intf/logging.h>

////////////////////////////////////////////////////////////
/// Docs

#define TODO_NEXT(msg)

#define TODO(msg)

#define LYING_CHECKSUM_FLAG(msg)

#define SUGGESTION(msg)

#define MAYBE_ASSERT(expr) ASSERT (expr)

////////////////////////////////////////////////////////////
/// PANIC
#define panic(msg)                      \
  do                                    \
    {                                   \
      i_log_error ("PANIC! %s\n", msg); \
      i_log_flush ();                   \
      crash ();                         \
    }                                   \
  while (0)

////////////////////////////////////////////////////////////
/// ASSERT
#define ASSERT(expr)                                          \
  do                                                          \
    {                                                         \
      if (!(expr))                                            \
        {                                                     \
          i_log_assert ("%s failed at %s:%d (%s)\n",          \
                        #expr, __FILE__, __LINE__, __func__); \
          i_log_flush ();                                     \
          crash ();                                           \
        }                                                     \
    }                                                         \
  while (0)

#define ASSERTF(expr, fmt, ...)                               \
  do                                                          \
    {                                                         \
      if (!(expr))                                            \
        {                                                     \
          i_log_assert ("%s failed at %s:%d (%s)\n",          \
                        #expr, __FILE__, __LINE__, __func__); \
          i_log_assert ("Message: " fmt, ##__VA_ARGS__);      \
          i_log_flush ();                                     \
          crash ();                                           \
        }                                                     \
    }                                                         \
  while (0)

#define ASSERT_NOERR(expr, ename)                             \
  do                                                          \
    {                                                         \
      error ename = error_create ();                          \
      if (expr < 0)                                           \
        {                                                     \
          i_log_assert ("%s failed at %s:%d (%s)\n",          \
                        #expr, __FILE__, __LINE__, __func__); \
          crash ();                                           \
        }                                                     \
    }                                                         \
  while (0)

#define DEFINE_DBG_ASSERT(type, name, var, body) \
  HEADER_FUNC void name##_assert__ (const type *var) body

#define DBG_ASSERT(name, expr) name##_assert__ (expr)

#define ASSERT_EXPR(expr) \
  do                      \
    {                     \
      expr                \
    }                     \
  while (0)
