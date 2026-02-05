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
#include <numstore/core/error.h>
#include <numstore/core/lalloc.h>
#include <numstore/core/llist.h>
#include <numstore/core/string.h>

////////////////////////////////////////////////////////////
//// Object and array literal types
struct object
{
  struct string *keys;
  struct literal *literals;
  u32 len;
};

struct array
{
  struct literal *literals;
  u32 len;
};

i32 object_t_snprintf (char *str, u32 size, const struct object *st);
bool object_equal (const struct object *left, const struct object *right);
err_t object_plus (struct object *dest, const struct object *right, struct lalloc *alloc, error *e);

bool array_equal (const struct array *left, const struct array *right);
err_t array_plus (struct array *dest, const struct array *right, struct lalloc *alloc, error *e);

////////////////////////////////////////////////////////////
//// Literal
enum literal_t
{
  /* Composite */
  LT_OBJECT,
  LT_ARRAY,

  /* Simple */
  LT_STRING,
  LT_INTEGER,
  LT_DECIMAL,
  LT_COMPLEX,
  LT_BOOL,
};

const char *literal_t_tostr (enum literal_t);
bool literal_equal (const struct literal *left, const struct literal *right);

struct literal
{
  enum literal_t type;

  union
  {
    bool bl;
    struct object obj;
    struct array arr;
    struct string str;
    i32 integer;
    f32 decimal;
    cf64 cplx;
  };
};

/////////////////////////
// Object / Array builders

struct object_llnode
{
  struct string key;
  struct literal v;
  struct llnode link;
};

struct array_llnode
{
  struct literal v;
  struct llnode link;
};

struct object_builder
{
  struct llnode *head;

  u16 klen;
  u16 tlen;

  struct lalloc *work;
  struct lalloc *dest;
};

struct array_builder
{
  struct llnode *head;

  struct lalloc *work;
  struct lalloc *dest;
};

struct object_builder objb_create (struct lalloc *work, struct lalloc *dest);
err_t objb_accept_string (struct object_builder *o, const struct string key, error *e);
err_t objb_accept_literal (struct object_builder *o, struct literal v, error *e);
err_t objb_build (struct object *dest, struct object_builder *b, error *e);

struct array_builder arb_create (struct lalloc *work, struct lalloc *dest);
err_t arb_accept_literal (struct array_builder *o, struct literal v, error *e);
err_t arb_build (struct array *dest, struct array_builder *b, error *e);

void i_log_literal (struct literal *v);

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
