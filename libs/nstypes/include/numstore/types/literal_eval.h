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
 *   Literal types and operations. Defines structures for primitive, string,
 *   array, and object literals with builder patterns for complex types.
 *   Provides operators for arithmetic, logical, and bitwise operations on
 *   literals.
 */

// core
#include "numstore/types/vbank.h"
#include <numstore/compiler/literal.h>
#include <numstore/core/error.h>
#include <numstore/core/lalloc.h>
#include <numstore/core/llist.h>
#include <numstore/core/string.h>

/////////////////////////
// Expression reductions

// dest = dest + right
err_t literal_plus_literal (struct literal *dest, const struct literal *right, struct lalloc *alloc, error *e);

// dest = dest - right
err_t literal_minus_literal (struct literal *dest, const struct literal *right, error *e);

// dest = dest * right
err_t literal_star_literal (struct literal *dest, const struct literal *right, error *e);

// dest = dest / right
err_t literal_slash_literal (struct literal *dest, const struct literal *right, error *e);

// dest = dest == right
err_t literal_equal_equal_literal (struct literal *dest, const struct literal *right, error *e);

// dest = dest != right
err_t literal_bang_equal_literal (struct literal *dest, const struct literal *right, error *e);

// dest = dest > right
err_t literal_greater_literal (struct literal *dest, const struct literal *right, error *e);

// dest = dest >= right
err_t literal_greater_equal_literal (struct literal *dest, const struct literal *right, error *e);

// dest = dest < right
err_t literal_less_literal (struct literal *dest, const struct literal *right, error *e);

// dest = dest <= right
err_t literal_less_equal_literal (struct literal *dest, const struct literal *right, error *e);

// dest = dest ^ right
err_t literal_caret_literal (struct literal *dest, const struct literal *right, error *e);

// dest = dest % right
err_t literal_mod_literal (struct literal *dest, const struct literal *right, error *e);

// dest = dest | right
err_t literal_pipe_literal (struct literal *dest, const struct literal *right, error *e);

// dest = dest || right
void literal_pipe_pipe_literal (struct literal *dest, const struct literal *right);

// dest = dest & right
err_t literal_ampersand_literal (struct literal *dest, const struct literal *right, error *e);

// dest = dest && right
void literal_ampersand_ampersand_literal (struct literal *dest, const struct literal *right);

// dest = ~dest
err_t literal_not (struct literal *dest, error *e);

// dest = -dest
err_t literal_minus (struct literal *dest, error *e);

// dest = !dest
void literal_bang (struct literal *dest);
