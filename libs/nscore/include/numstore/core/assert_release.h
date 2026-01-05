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
 *   Release-mode assertion stubs that compile to no-ops. Used in production
 *   builds to eliminate assertion overhead.
 */

// core
#include <numstore/core/macros.h>
#include <numstore/core/signatures.h>

/* Throw a compile time error to discourage various debug macros */
#define NOT_FOR_PRODUCTION() // typedef char PANIC_in_release_mode_is_not_allowed[-1]

/* Release doesn't allow these two */
#define panic(msg) NOT_FOR_PRODUCTION ()
#define ASSERT(expr)
#define ASSERTF(expr, ...)

#define DEFINE_DBG_ASSERT(type, name, var, body) \
  HEADER_FUNC void name##_assert (const type *var) { (void)var; }

#define DBG_ASSERT(name, expr) ((void)0)

#define ASSERT_EXPR(expr)

#define TODO_NEXT(msg)

#define TODO(msg)

#define LYING_CHECKSUM_FLAG(msg)

#define SUGGESTION(msg)

#define MAYBE_ASSERT(expr) ASSERT (expr)
