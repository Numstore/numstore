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
 *   Implements literal.h. Provides implementation of literal operations,
 *   object and array builders, and arithmetic/logical operators for all
 *   literal types.
 */

#include <numstore/types/literal.h>

#include <numstore/core/assert.h>
#include <numstore/core/math.h>
#include <numstore/core/string.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/stdlib.h>
#include <numstore/test/testing.h>

#define BOOL_NUMBER_TYPE_COERCE(dest, OP, right)                      \
  do                                                                  \
    {                                                                 \
      /* int + int */                                                 \
      if (dest->type == LT_INTEGER && right->type == LT_INTEGER)      \
        {                                                             \
          dest->bl = dest->integer OP right->integer;                 \
          dest->type = LT_BOOL;                                       \
          return SUCCESS;                                             \
        }                                                             \
                                                                      \
      /* dec + dec */                                                 \
      else if (dest->type == LT_DECIMAL && right->type == LT_DECIMAL) \
        {                                                             \
          dest->bl = dest->decimal OP right->decimal;                 \
          dest->type = LT_BOOL;                                       \
          return SUCCESS;                                             \
        }                                                             \
                                                                      \
      /* cplx + dec */                                                \
      else if (dest->type == LT_COMPLEX && right->type == LT_DECIMAL) \
        {                                                             \
          dest->bl = dest->cplx OP (cf128) right->decimal;            \
          dest->type = LT_BOOL;                                       \
          return SUCCESS;                                             \
        }                                                             \
                                                                      \
      /* cplx + cplx */                                               \
      else if (dest->type == LT_COMPLEX && right->type == LT_COMPLEX) \
        {                                                             \
          dest->bl = dest->cplx OP right->cplx;                       \
          dest->type = LT_BOOL;                                       \
          return SUCCESS;                                             \
        }                                                             \
                                                                      \
      /* dec + cplx */                                                \
      else if (dest->type == LT_DECIMAL && right->type == LT_COMPLEX) \
        {                                                             \
          dest->bl = (cf128)dest->decimal OP right->cplx;             \
          dest->type = LT_BOOL;                                       \
          return SUCCESS;                                             \
        }                                                             \
                                                                      \
      /* int + dec */                                                 \
      else if (dest->type == LT_INTEGER && right->type == LT_DECIMAL) \
        {                                                             \
          dest->bl = (f64)dest->integer OP right->decimal;            \
          dest->type = LT_BOOL;                                       \
          return SUCCESS;                                             \
        }                                                             \
                                                                      \
      /* dec + int */                                                 \
      else if (dest->type == LT_DECIMAL && right->type == LT_INTEGER) \
        {                                                             \
          dest->bl = dest->decimal OP (f64) right->integer;           \
          dest->type = LT_BOOL;                                       \
          return SUCCESS;                                             \
        }                                                             \
                                                                      \
      /* int + cplx */                                                \
      else if (dest->type == LT_INTEGER && right->type == LT_COMPLEX) \
        {                                                             \
          dest->bl = (cf128)dest->integer OP right->cplx;             \
          dest->type = LT_BOOL;                                       \
          return SUCCESS;                                             \
        }                                                             \
                                                                      \
      /* cplx + int */                                                \
      else if (dest->type == LT_COMPLEX && right->type == LT_INTEGER) \
        {                                                             \
          dest->bl = dest->cplx OP (cf128) right->integer;            \
          dest->type = LT_BOOL;                                       \
          return SUCCESS;                                             \
        }                                                             \
    }                                                                 \
  while (0)

#define BOOL_NUMBER_TYPE_COERCE_CABS(dest, OP, right)                                   \
  do                                                                                    \
    {                                                                                   \
      /* int OP int -> bool */                                                          \
      if ((dest)->type == LT_INTEGER && (right)->type == LT_INTEGER)                    \
        {                                                                               \
          (dest)->bl = (dest)->integer OP (right)->integer;                             \
          (dest)->type = LT_BOOL;                                                       \
          return SUCCESS;                                                               \
        }                                                                               \
                                                                                        \
      /* dec OP dec -> bool */                                                          \
      else if ((dest)->type == LT_DECIMAL && (right)->type == LT_DECIMAL)               \
        {                                                                               \
          (dest)->bl = (dest)->decimal OP (right)->decimal;                             \
          (dest)->type = LT_BOOL;                                                       \
          return SUCCESS;                                                               \
        }                                                                               \
                                                                                        \
      /* |cmplx| OP |cmplx| -> bool */                                                  \
      else if ((dest)->type == LT_COMPLEX && (right)->type == LT_COMPLEX)               \
        {                                                                               \
          (dest)->bl = i_cabs_sqrd_64 ((dest)->cplx) OP i_cabs_sqrd_64 ((right)->cplx); \
          (dest)->type = LT_BOOL;                                                       \
          return SUCCESS;                                                               \
        }                                                                               \
      /* dec OP int -> bool */                                                          \
      else if ((dest)->type == LT_DECIMAL && (right)->type == LT_INTEGER)               \
        {                                                                               \
          (dest)->bl = (dest)->decimal OP (right)->integer;                             \
          (dest)->type = LT_BOOL;                                                       \
          return SUCCESS;                                                               \
        }                                                                               \
                                                                                        \
      /* int OP dec -> bool */                                                          \
      else if ((dest)->type == LT_INTEGER && (right)->type == LT_DECIMAL)               \
        {                                                                               \
          (dest)->bl = (dest)->integer OP (right)->decimal;                             \
          (dest)->type = LT_BOOL;                                                       \
          return SUCCESS;                                                               \
        }                                                                               \
                                                                                        \
      /* int OP |cmplx| -> bool */                                                      \
      else if ((dest)->type == LT_INTEGER && (right)->type == LT_COMPLEX)               \
        {                                                                               \
          (dest)->bl = (dest)->integer OP i_cabs_sqrd_64 ((right)->cplx);               \
          (dest)->type = LT_BOOL;                                                       \
          return SUCCESS;                                                               \
        }                                                                               \
                                                                                        \
      /* dec OP |cmplx| -> bool */                                                      \
      else if ((dest)->type == LT_DECIMAL && (right)->type == LT_COMPLEX)               \
        {                                                                               \
          (dest)->bl = (dest)->decimal OP i_cabs_sqrd_64 ((right)->cplx);               \
          (dest)->type = LT_BOOL;                                                       \
          return SUCCESS;                                                               \
        }                                                                               \
    }                                                                                   \
  while (0)

#define SCALAR_NUMBER_TYPE_COERCE(dest, OP, right)                        \
  do                                                                      \
    {                                                                     \
      /* int + int */                                                     \
      if ((dest)->type == LT_INTEGER && (right)->type == LT_INTEGER)      \
        {                                                                 \
          (dest)->integer = (dest)->integer OP (right)->integer;          \
          return SUCCESS;                                                 \
        }                                                                 \
                                                                          \
      /* dec + dec */                                                     \
      else if ((dest)->type == LT_DECIMAL && (right)->type == LT_DECIMAL) \
        {                                                                 \
          (dest)->decimal = (dest)->decimal OP (right)->decimal;          \
          return SUCCESS;                                                 \
        }                                                                 \
                                                                          \
      /* cmplx + cmplx */                                                 \
      else if ((dest)->type == LT_COMPLEX && (right)->type == LT_COMPLEX) \
        {                                                                 \
          (dest)->cplx = (dest)->cplx OP (right)->cplx;                   \
          return SUCCESS;                                                 \
        }                                                                 \
                                                                          \
      /* cmplx + dec */                                                   \
      else if ((dest)->type == LT_COMPLEX && (right)->type == LT_DECIMAL) \
        {                                                                 \
          (dest)->cplx = (dest)->cplx OP (right)->decimal;                \
          return SUCCESS;                                                 \
        }                                                                 \
                                                                          \
      /* cmplx + int */                                                   \
      else if ((dest)->type == LT_COMPLEX && (right)->type == LT_INTEGER) \
        {                                                                 \
          (dest)->cplx = (dest)->cplx OP (right)->integer;                \
          return SUCCESS;                                                 \
        }                                                                 \
                                                                          \
      /* dec + int */                                                     \
      else if ((dest)->type == LT_DECIMAL && (right)->type == LT_INTEGER) \
        {                                                                 \
          (dest)->decimal = (dest)->decimal OP (right)->integer;          \
          return SUCCESS;                                                 \
        }                                                                 \
                                                                          \
      /* dec + cmplx -> promote to complex */                             \
      else if ((dest)->type == LT_DECIMAL && (right)->type == LT_COMPLEX) \
        {                                                                 \
          cf128 tmp = (dest)->decimal;                                    \
          tmp = tmp OP (right)->cplx;                                     \
          (dest)->cplx = tmp;                                             \
          (dest)->type = LT_COMPLEX;                                      \
          return SUCCESS;                                                 \
        }                                                                 \
                                                                          \
      /* int + dec -> promote to decimal */                               \
      else if ((dest)->type == LT_INTEGER && (right)->type == LT_DECIMAL) \
        {                                                                 \
          (dest)->decimal = (dest)->integer OP (right)->decimal;          \
          (dest)->type = LT_DECIMAL;                                      \
          return SUCCESS;                                                 \
        }                                                                 \
                                                                          \
      /* int + cmplx -> promote to complex */                             \
      else if ((dest)->type == LT_INTEGER && (right)->type == LT_COMPLEX) \
        {                                                                 \
          cf128 tmp = (dest)->integer;                                    \
          tmp = tmp OP (right)->cplx;                                     \
          (dest)->cplx = tmp;                                             \
          (dest)->type = LT_COMPLEX;                                      \
          return SUCCESS;                                                 \
        }                                                                 \
    }                                                                     \
  while (0)

#define BITMANIP_TYPE_COERCE(dest, OP, right)                             \
  do                                                                      \
    {                                                                     \
      /* bool OP bool => bool */                                          \
      if ((dest)->type == LT_BOOL && (right)->type == LT_BOOL)            \
        {                                                                 \
          (dest)->bl = (dest)->bl OP (right)->bl;                         \
          return SUCCESS;                                                 \
        }                                                                 \
                                                                          \
      /* int OP bool => int */                                            \
      else if ((dest)->type == LT_INTEGER && (right)->type == LT_BOOL)    \
        {                                                                 \
          (dest)->integer = (dest)->integer OP (right)->bl;               \
          return SUCCESS;                                                 \
        }                                                                 \
                                                                          \
      /* bool OP int => int */                                            \
      else if ((dest)->type == LT_BOOL && (right)->type == LT_INTEGER)    \
        {                                                                 \
          (dest)->integer = (dest)->bl OP (right)->integer;               \
          (dest)->type = LT_INTEGER;                                      \
          return SUCCESS;                                                 \
        }                                                                 \
                                                                          \
      /* int OP int => int */                                             \
      else if ((dest)->type == LT_INTEGER && (right)->type == LT_INTEGER) \
        {                                                                 \
          (dest)->integer = (dest)->integer OP (right)->integer;          \
          return SUCCESS;                                                 \
        }                                                                 \
    }                                                                     \
  while (0)

#define ERR_UNSUPPORTED_BIN_OP(left, right, op)   \
  error_causef (                                  \
      e, ERR_SYNTAX,                              \
      "Unsupported operation type: %s for %s %s", \
      op, literal_t_tostr (left->type),           \
      literal_t_tostr (right->type))

#define ERR_UNSUPPORTED_UN_OP(dest, op)        \
  error_causef (                               \
      e, ERR_SYNTAX,                           \
      "Unsupported operation type: %s for %s", \
      op, literal_t_tostr (dest->type))

static inline bool
literal_truthy (const struct literal *v)
{
  switch (v->type)
    {
    case LT_BOOL:
      {
        return v->bl;
      }
    case LT_INTEGER:
      {
        return v->integer != 0.0;
      }
    case LT_DECIMAL:
      {
        return v->decimal != 0.0;
      }
    case LT_COMPLEX:
      {
        return i_cabs_sqrd_64 (v->cplx) != 0.0;
      }
    case LT_STRING:
      {
        return v->str.len != 0;
      }
    case LT_OBJECT:
      {
        return v->obj.len != 0;
      }
    case LT_ARRAY:
      {
        return v->arr.len != 0;
      }
    default:
      {
        UNREACHABLE ();
      }
    }
}

#ifndef NTEST
TEST (TT_UNIT, literal_truthy)
{
  struct literal v;

  v = (struct literal){ .type = LT_BOOL, .bl = true };
  test_assert (literal_truthy (&v));

  v = (struct literal){ .type = LT_BOOL, .bl = false };
  test_assert (!literal_truthy (&v));

  v = (struct literal){ .type = LT_INTEGER, .integer = 0 };
  test_assert (!literal_truthy (&v));

  v = (struct literal){ .type = LT_INTEGER, .integer = 3 };
  test_assert (literal_truthy (&v));

  v = (struct literal){ .type = LT_DECIMAL, .decimal = 0.0 };
  test_assert (!literal_truthy (&v));

  v = (struct literal){ .type = LT_DECIMAL, .decimal = 3.14 };
  test_assert (literal_truthy (&v));

  v = (struct literal){ .type = LT_COMPLEX, .cplx = 0 + 0 * I };
  test_assert (!literal_truthy (&v));

  v = (struct literal){ .type = LT_COMPLEX, .cplx = 3 + 4 * I }; /* magnitude = 5 */
  test_assert (literal_truthy (&v));

  v = (struct literal){ .type = LT_STRING, .str = { .data = "", .len = 0 } };
  test_assert (!literal_truthy (&v));

  v = (struct literal){ .type = LT_STRING, .str = { .data = "hi", .len = 2 } };
  test_assert (literal_truthy (&v));

  v = (struct literal){ .type = LT_OBJECT, .obj = { .len = 0 } };
  test_assert (!literal_truthy (&v));

  v = (struct literal){ .type = LT_OBJECT, .obj = { .len = 2 } };
  test_assert (literal_truthy (&v));

  v = (struct literal){ .type = LT_ARRAY, .arr = { .len = 0 } };
  test_assert (!literal_truthy (&v));

  v = (struct literal){ .type = LT_ARRAY, .arr = { .len = 5 } };
  test_assert (literal_truthy (&v));
}
#endif

err_t
literal_plus_literal (struct literal *dest, const struct literal *right, struct lalloc *alc, error *e)
{
  /* First check for number / complex */
  SCALAR_NUMBER_TYPE_COERCE (dest, +, right);

  /* ARRAY + ARRAY */
  if (dest->type == LT_ARRAY && right->type == LT_ARRAY)
    {
      return array_plus (&dest->arr, &right->arr, alc, e);
    }

  /* OBJECT + OBJECT */
  else if (dest->type == LT_OBJECT && right->type == LT_OBJECT)
    {
      return object_plus (&dest->obj, &right->obj, alc, e);
    }

  /* STRING + STRING */
  else if (dest->type == LT_STRING && right->type == LT_STRING)
    {
      struct string next = string_plus (dest->str, right->str, alc, e);
      if (e->cause_code)
        {
          return e->cause_code;
        }
      dest->str = next;
      return SUCCESS;
    }

  return ERR_UNSUPPORTED_BIN_OP (dest, right, "+");
}

#ifndef NTEST
typedef struct
{
  enum literal_t lhs, rhs;
} literal_pair;

#ifndef NTEST
TEST (TT_UNIT, literal_plus_literal)
{
  u8 data[2048];
  struct lalloc alc = lalloc_create_from (data);
  error err = error_create ();

  struct literal a, b;

  TEST_CASE ("String + String")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("foo") };
    b = (struct literal){ .type = LT_STRING, .str = strfcstr ("bar") };
    test_assert (literal_plus_literal (&a, &b, &alc, &err) == SUCCESS);
    test_assert (a.str.len == 6);
    test_assert (memcmp (a.str.data, "foobar", 6) == 0);
  }

  TEST_CASE ("Integer + Integer")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 3 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 4 };
    test_assert (literal_plus_literal (&a, &b, &alc, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == 7);
  }

  TEST_CASE ("Decimal + Decimal")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 1.25 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 2.75 };
    test_assert (literal_plus_literal (&a, &b, &alc, &err) == SUCCESS);
    test_assert (a.type == LT_DECIMAL);
    test_assert (i_fabs_32 (a.decimal - 4.0f) < 1e-12);
  }

  TEST_CASE ("Integer + Decimal")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 5.5 };
    test_assert (literal_plus_literal (&a, &b, &alc, &err) == SUCCESS);
    test_assert (a.type == LT_DECIMAL);
    test_assert (i_fabs_32 (a.decimal - 7.5f) < 1e-12);
  }

  TEST_CASE ("Decimal + Integer")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 3.5 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    test_assert (literal_plus_literal (&a, &b, &alc, &err) == SUCCESS);
    test_assert (a.type == LT_DECIMAL);
    test_assert (i_fabs_32 (a.decimal - 5.5f) < 1e-12);
  }

  TEST_CASE ("Complex + Complex")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 2.0 * I };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 3.0 + 4.0 * I };
    test_assert (literal_plus_literal (&a, &b, &alc, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) == 4.0);
    test_assert (i_cimag_64 (a.cplx) == 6.0);
  }

  TEST_CASE ("Complex + Integer")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 2.0 * I };
    b = (struct literal){ .type = LT_INTEGER, .integer = 5 };
    test_assert (literal_plus_literal (&a, &b, &alc, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) == 6.0);
    test_assert (i_cimag_64 (a.cplx) == 2.0);
  }

  TEST_CASE ("Complex + Decimal")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = -1.0 + 0.5 * I };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 2.5 };
    test_assert (literal_plus_literal (&a, &b, &alc, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) == 1.5);
    test_assert (i_cimag_64 (a.cplx) == 0.5);
  }

  TEST_CASE ("Integer + Complex")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 0.0 + 3.0 * I };
    test_assert (literal_plus_literal (&a, &b, &alc, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) == 2.0);
    test_assert (i_cimag_64 (a.cplx) == 3.0);
  }

  TEST_CASE ("Decimal + Complex")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 4.75 };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = -1.0 + 0.25 * I };
    test_assert (literal_plus_literal (&a, &b, &alc, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) == 3.75);
    test_assert (i_cimag_64 (a.cplx) == 0.25);
  }

  literal_pair invalid_combos[] = {
    { LT_BOOL, LT_BOOL },
    { LT_BOOL, LT_STRING },
    { LT_BOOL, LT_COMPLEX },
    { LT_BOOL, LT_OBJECT },
    { LT_BOOL, LT_ARRAY },
    { LT_BOOL, LT_INTEGER },
    { LT_BOOL, LT_DECIMAL },

    { LT_STRING, LT_BOOL },
    { LT_STRING, LT_COMPLEX },
    { LT_STRING, LT_OBJECT },
    { LT_STRING, LT_ARRAY },
    { LT_STRING, LT_INTEGER },
    { LT_STRING, LT_DECIMAL },

    { LT_INTEGER, LT_BOOL },
    { LT_INTEGER, LT_STRING },
    { LT_INTEGER, LT_OBJECT },
    { LT_INTEGER, LT_ARRAY },

    { LT_DECIMAL, LT_BOOL },
    { LT_DECIMAL, LT_STRING },
    { LT_DECIMAL, LT_OBJECT },
    { LT_DECIMAL, LT_ARRAY },

    { LT_COMPLEX, LT_BOOL },
    { LT_COMPLEX, LT_STRING },
    { LT_COMPLEX, LT_OBJECT },
    { LT_COMPLEX, LT_ARRAY },

    { LT_OBJECT, LT_BOOL },
    { LT_OBJECT, LT_STRING },
    { LT_OBJECT, LT_INTEGER },
    { LT_OBJECT, LT_DECIMAL },
    { LT_OBJECT, LT_COMPLEX },
    { LT_OBJECT, LT_ARRAY },

    { LT_ARRAY, LT_BOOL },
    { LT_ARRAY, LT_STRING },
    { LT_ARRAY, LT_INTEGER },
    { LT_ARRAY, LT_DECIMAL },
    { LT_ARRAY, LT_COMPLEX },
    { LT_ARRAY, LT_OBJECT },
  };

  for (u32 i = 0; i < sizeof (invalid_combos) / sizeof (invalid_combos[0]); i++)
    {
      a = (struct literal){ .type = invalid_combos[i].lhs };
      b = (struct literal){ .type = invalid_combos[i].rhs };
      err_t ret = literal_plus_literal (&a, &b, &alc, &err);
      if (ret != ERR_SYNTAX)
        {
          i_log_failure ("Expected plus to return syntax error: %s %s\n", literal_t_tostr (a.type), literal_t_tostr (b.type));
        }
      test_assert (ret == ERR_SYNTAX);
      err.cause_code = SUCCESS;
    }
}
#endif
#endif

err_t
literal_minus_literal (struct literal *dest, const struct literal *right, error *e)
{
  SCALAR_NUMBER_TYPE_COERCE (dest, -, right);
  return ERR_UNSUPPORTED_BIN_OP (dest, right, "-");
}

#ifndef NTEST
TEST (TT_UNIT, literal_minus_literal)
{
  error err = error_create ();
  struct literal a, b;

  TEST_CASE ("Integer - Integer")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 5 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    test_assert (literal_minus_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == 3);
  }

  TEST_CASE ("Decimal - Decimal")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 5.5 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 1.25 };
    test_assert (literal_minus_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_DECIMAL);
    test_assert (i_fabs_32 (a.decimal - 4.25f) < 1e-12);
  }

  TEST_CASE ("Integer - Decimal")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 3 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 0.5 };
    test_assert (literal_minus_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_DECIMAL);
    test_assert (i_fabs_32 (a.decimal - 2.5f) < 1e-12);
  }

  TEST_CASE ("Decimal - Integer")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 4.0 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 1 };
    test_assert (literal_minus_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_DECIMAL);
    test_assert (i_fabs_32 (a.decimal - 3.0f) < 1e-12);
  }

  TEST_CASE ("Complex - Complex")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 5.0 + 6.0 * I };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 2.0 * I };
    test_assert (literal_minus_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) == 4.0);
    test_assert (i_cimag_64 (a.cplx) == 4.0);
  }

  TEST_CASE ("Complex - Integer")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 2.0 + 3.0 * I };
    b = (struct literal){ .type = LT_INTEGER, .integer = 1 };
    test_assert (literal_minus_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) == 1.0);
    test_assert (i_cimag_64 (a.cplx) == 3.0);
  }

  TEST_CASE ("Complex - Decimal")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 4.0 + 2.0 * I };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 1.5 };
    test_assert (literal_minus_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) == 2.5);
    test_assert (i_cimag_64 (a.cplx) == 2.0);
  }

  TEST_CASE ("Integer - Complex")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 3 };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 1.0 * I };
    test_assert (literal_minus_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) == 2.0);
    test_assert (i_cimag_64 (a.cplx) == -1.0);
  }

  TEST_CASE ("Decimal - Complex")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 3.0 };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 0.5 * I };
    test_assert (literal_minus_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) == 2.0);
    test_assert (i_cimag_64 (a.cplx) == -0.5);
  }

  TEST_CASE ("Object - Object")
  {
    /* Array - Array */
  }

  literal_pair invalid_combos[] = {
    { LT_BOOL, LT_BOOL },
    { LT_BOOL, LT_STRING },
    { LT_BOOL, LT_COMPLEX },
    { LT_BOOL, LT_OBJECT },
    { LT_BOOL, LT_ARRAY },
    { LT_BOOL, LT_INTEGER },
    { LT_BOOL, LT_DECIMAL },

    { LT_STRING, LT_BOOL },
    { LT_STRING, LT_COMPLEX },
    { LT_STRING, LT_OBJECT },
    { LT_STRING, LT_ARRAY },
    { LT_STRING, LT_INTEGER },
    { LT_STRING, LT_DECIMAL },

    { LT_INTEGER, LT_BOOL },
    { LT_INTEGER, LT_STRING },
    { LT_INTEGER, LT_OBJECT },
    { LT_INTEGER, LT_ARRAY },

    { LT_DECIMAL, LT_BOOL },
    { LT_DECIMAL, LT_STRING },
    { LT_DECIMAL, LT_OBJECT },
    { LT_DECIMAL, LT_ARRAY },

    { LT_COMPLEX, LT_BOOL },
    { LT_COMPLEX, LT_STRING },
    { LT_COMPLEX, LT_OBJECT },
    { LT_COMPLEX, LT_ARRAY },

    { LT_OBJECT, LT_BOOL },
    { LT_OBJECT, LT_STRING },
    { LT_OBJECT, LT_INTEGER },
    { LT_OBJECT, LT_DECIMAL },
    { LT_OBJECT, LT_COMPLEX },
    { LT_OBJECT, LT_ARRAY },

    { LT_ARRAY, LT_BOOL },
    { LT_ARRAY, LT_STRING },
    { LT_ARRAY, LT_INTEGER },
    { LT_ARRAY, LT_DECIMAL },
    { LT_ARRAY, LT_COMPLEX },
    { LT_ARRAY, LT_OBJECT },
  };

  for (u32 i = 0; i < sizeof (invalid_combos) / sizeof (invalid_combos[0]); i++)
    {
      a = (struct literal){ .type = invalid_combos[i].lhs };
      b = (struct literal){ .type = invalid_combos[i].rhs };
      err_t ret = literal_minus_literal (&a, &b, &err);
      if (ret != ERR_SYNTAX)
        {
          i_log_failure ("Expected minus to return syntax error: %s %s\n", literal_t_tostr (a.type), literal_t_tostr (b.type));
        }
      test_assert (ret == ERR_SYNTAX);
      err.cause_code = SUCCESS;
    }
}
#endif

err_t
literal_star_literal (struct literal *dest, const struct literal *right, error *e)
{
  SCALAR_NUMBER_TYPE_COERCE (dest, *, right);

  return ERR_UNSUPPORTED_BIN_OP (dest, right, "*");
}

#ifndef NTEST
TEST (TT_UNIT, literal_star_literal)
{
  error err = error_create ();
  struct literal a, b;
  TEST_CASE ("Integer * Integer")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 3 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 4 };
    test_assert (literal_star_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == 12);
  }

  TEST_CASE ("Decimal * Decimal")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 1.5 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 2.0 };
    test_assert (literal_star_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_DECIMAL);
    test_assert (i_fabs_32 (a.decimal - 3.0f) < 1e-12);
  }

  TEST_CASE ("Integer * Decimal")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 4.5 };
    test_assert (literal_star_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_DECIMAL);
    test_assert (i_fabs_32 (a.decimal - 9.0f) < 1e-12);
  }

  TEST_CASE ("Decimal * Integer")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 2.5 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    test_assert (literal_star_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_DECIMAL);
    test_assert (i_fabs_32 (a.decimal - 5.0f) < 1e-12);
  }

  TEST_CASE ("Complex * Complex")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 2.0 * I };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 2.0 + 3.0 * I };
    test_assert (literal_star_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) == -4.0);
    test_assert (i_cimag_64 (a.cplx) == 7.0);
  }

  TEST_CASE ("Complex * Integer")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 1.5 + 0.5 * I };
    b = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    test_assert (literal_star_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) == 3.0);
    test_assert (i_cimag_64 (a.cplx) == 1.0);
  }

  TEST_CASE ("Complex * Decimal")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 2.0 + 3.0 * I };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 2.0 };
    test_assert (literal_star_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) == 4.0);
    test_assert (i_cimag_64 (a.cplx) == 6.0);
  }

  TEST_CASE ("Integer * Complex")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 3.0 + 4.0 * I };
    test_assert (literal_star_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) == 6.0);
    test_assert (i_cimag_64 (a.cplx) == 8.0);
  }

  TEST_CASE ("Decimal * Complex")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 1.5 };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 4.0 + 2.0 * I };
    test_assert (literal_star_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) == 6.0);
    test_assert (i_cimag_64 (a.cplx) == 3.0);
  }

  TEST_CASE ("Object + Object")
  {
    /* Array + Array */
  }

  literal_pair invalid_combos[] = {
    { LT_BOOL, LT_BOOL },
    { LT_BOOL, LT_STRING },
    { LT_BOOL, LT_COMPLEX },
    { LT_BOOL, LT_OBJECT },
    { LT_BOOL, LT_ARRAY },
    { LT_BOOL, LT_INTEGER },
    { LT_BOOL, LT_DECIMAL },

    { LT_STRING, LT_BOOL },
    { LT_STRING, LT_COMPLEX },
    { LT_STRING, LT_OBJECT },
    { LT_STRING, LT_ARRAY },
    { LT_STRING, LT_INTEGER },
    { LT_STRING, LT_DECIMAL },

    { LT_INTEGER, LT_BOOL },
    { LT_INTEGER, LT_STRING },
    { LT_INTEGER, LT_OBJECT },
    { LT_INTEGER, LT_ARRAY },

    { LT_DECIMAL, LT_BOOL },
    { LT_DECIMAL, LT_STRING },
    { LT_DECIMAL, LT_OBJECT },
    { LT_DECIMAL, LT_ARRAY },

    { LT_COMPLEX, LT_BOOL },
    { LT_COMPLEX, LT_STRING },
    { LT_COMPLEX, LT_OBJECT },
    { LT_COMPLEX, LT_ARRAY },

    { LT_OBJECT, LT_BOOL },
    { LT_OBJECT, LT_STRING },
    { LT_OBJECT, LT_INTEGER },
    { LT_OBJECT, LT_DECIMAL },
    { LT_OBJECT, LT_COMPLEX },
    { LT_OBJECT, LT_ARRAY },

    { LT_ARRAY, LT_BOOL },
    { LT_ARRAY, LT_STRING },
    { LT_ARRAY, LT_INTEGER },
    { LT_ARRAY, LT_DECIMAL },
    { LT_ARRAY, LT_COMPLEX },
    { LT_ARRAY, LT_OBJECT },
  };

  for (u32 i = 0; i < sizeof (invalid_combos) / sizeof (invalid_combos[0]); i++)
    {
      a = (struct literal){ .type = invalid_combos[i].lhs };
      b = (struct literal){ .type = invalid_combos[i].rhs };
      err_t ret = literal_star_literal (&a, &b, &err);
      if (ret != ERR_SYNTAX)
        {
          i_log_failure ("Expected star to return syntax error: %s %s\n", literal_t_tostr (a.type), literal_t_tostr (b.type));
        }
      test_assert (ret == ERR_SYNTAX);
      err.cause_code = SUCCESS;
    }
}
#endif

err_t
literal_slash_literal (struct literal *dest, const struct literal *right, error *e)
{
  SCALAR_NUMBER_TYPE_COERCE (dest, /, right);

  return ERR_UNSUPPORTED_BIN_OP (dest, right, "/");
}

#ifndef NTEST
TEST (TT_UNIT, literal_slash_literal)
{
  error err = error_create ();
  struct literal a, b;
  TEST_CASE ("Integer / Integer")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 10 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    test_assert (literal_slash_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == 5);
  }

  TEST_CASE ("Decimal / Decimal")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 9.0 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 3.0 };
    test_assert (literal_slash_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_DECIMAL);
    test_assert (i_fabs_32 (a.decimal - 3.0f) < 1e-12);
  }

  TEST_CASE ("Integer / Decimal")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 6 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 2.0 };
    test_assert (literal_slash_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_DECIMAL);
    test_assert (i_fabs_32 (a.decimal - 3.0f) < 1e-12);
  }

  TEST_CASE ("Decimal / Integer")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 4.5 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 3 };
    test_assert (literal_slash_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_DECIMAL);
    test_assert (i_fabs_32 (a.decimal - 1.5f) < 1e-12);
  }

  TEST_CASE ("Complex / Complex")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 4.0 + 2.0 * I };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 1.0 * I };
    test_assert (literal_slash_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) != 0.0 || i_cimag_64 (a.cplx) != 0.0); /* sanity check */
  }

  TEST_CASE ("Complex / Integer")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 6.0 + 2.0 * I };
    b = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    test_assert (literal_slash_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) == 3.0);
    test_assert (i_cimag_64 (a.cplx) == 1.0);
  }

  TEST_CASE ("Complex / Decimal")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 8.0 + 4.0 * I };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 2.0 };
    test_assert (literal_slash_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) == 4.0);
    test_assert (i_cimag_64 (a.cplx) == 2.0);
  }

  TEST_CASE ("Integer / Complex")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 4 };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 1.0 * I };
    test_assert (literal_slash_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
  }

  TEST_CASE ("Decimal / Complex")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 3.0 };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 - 1.0 * I };
    test_assert (literal_slash_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
  }

  TEST_CASE ("Object + Object")
  {
    /* Array + Array */
  }

  literal_pair invalid_combos[] = {
    { LT_BOOL, LT_BOOL },
    { LT_BOOL, LT_STRING },
    { LT_BOOL, LT_COMPLEX },
    { LT_BOOL, LT_OBJECT },
    { LT_BOOL, LT_ARRAY },
    { LT_BOOL, LT_INTEGER },
    { LT_BOOL, LT_DECIMAL },

    { LT_STRING, LT_BOOL },
    { LT_STRING, LT_COMPLEX },
    { LT_STRING, LT_OBJECT },
    { LT_STRING, LT_ARRAY },
    { LT_STRING, LT_INTEGER },
    { LT_STRING, LT_DECIMAL },

    { LT_INTEGER, LT_BOOL },
    { LT_INTEGER, LT_STRING },
    { LT_INTEGER, LT_OBJECT },
    { LT_INTEGER, LT_ARRAY },

    { LT_DECIMAL, LT_BOOL },
    { LT_DECIMAL, LT_STRING },
    { LT_DECIMAL, LT_OBJECT },
    { LT_DECIMAL, LT_ARRAY },

    { LT_COMPLEX, LT_BOOL },
    { LT_COMPLEX, LT_STRING },
    { LT_COMPLEX, LT_OBJECT },
    { LT_COMPLEX, LT_ARRAY },

    { LT_OBJECT, LT_BOOL },
    { LT_OBJECT, LT_STRING },
    { LT_OBJECT, LT_INTEGER },
    { LT_OBJECT, LT_DECIMAL },
    { LT_OBJECT, LT_COMPLEX },
    { LT_OBJECT, LT_ARRAY },

    { LT_ARRAY, LT_BOOL },
    { LT_ARRAY, LT_STRING },
    { LT_ARRAY, LT_INTEGER },
    { LT_ARRAY, LT_DECIMAL },
    { LT_ARRAY, LT_COMPLEX },
    { LT_ARRAY, LT_OBJECT },
  };

  for (u32 i = 0; i < sizeof (invalid_combos) / sizeof (invalid_combos[0]); i++)
    {
      a = (struct literal){ .type = invalid_combos[i].lhs };
      b = (struct literal){ .type = invalid_combos[i].rhs };
      err_t ret = literal_slash_literal (&a, &b, &err);
      if (ret != ERR_SYNTAX)
        {
          i_log_failure ("Expected slash to return syntax error: %s %s\n", literal_t_tostr (a.type), literal_t_tostr (b.type));
        }
      test_assert (ret == ERR_SYNTAX);
      err.cause_code = SUCCESS;
    }
}
#endif

err_t
literal_equal_equal_literal (struct literal *dest, const struct literal *right, error *e)
{
  BOOL_NUMBER_TYPE_COERCE (dest, ==, right);

  if (dest->type == LT_STRING && right->type == LT_STRING)
    {
      dest->bl = string_equal (dest->str, right->str);
      dest->type = LT_BOOL;
      return SUCCESS;
    }
  else if (dest->type == LT_OBJECT && right->type == LT_OBJECT)
    {
      dest->bl = object_equal (&dest->obj, &right->obj);
      dest->type = LT_BOOL;
      return SUCCESS;
    }
  else if (dest->type == LT_ARRAY && right->type == LT_ARRAY)
    {
      dest->bl = array_equal (&dest->arr, &right->arr);
      dest->type = LT_BOOL;
      return SUCCESS;
    }
  else if (dest->type == LT_BOOL && right->type == LT_BOOL)
    {
      dest->bl = dest->bl == right->bl;
      dest->type = LT_BOOL;
      return SUCCESS;
    }

  return ERR_UNSUPPORTED_BIN_OP (dest, right, "==");
}

#ifndef NTEST
TEST (TT_UNIT, literal_equal_equal_literal)
{
  error err = error_create ();
  struct literal a, b;
  TEST_CASE ("TRUE == TRUE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    test_assert (literal_equal_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("TRUE == FALSE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    test_assert (literal_equal_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("FALSE == FALSE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = false };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    test_assert (literal_equal_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Integer == Integer (equal)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 3 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 3 };
    test_assert (literal_equal_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Integer == Integer (not equal)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 3 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    test_assert (literal_equal_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Decimal == Decimal (equal)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 2.5 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 2.5 };
    test_assert (literal_equal_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Decimal == Decimal (not equal)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 2.5 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 1.0 };
    test_assert (literal_equal_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Integer == Decimal (equal)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 2.0 };
    test_assert (literal_equal_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Decimal == Integer (not equal)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 3.0 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 5 };
    test_assert (literal_equal_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Complex == Complex (equal)")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 2.0 * I };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 2.0 * I };
    test_assert (literal_equal_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Complex == Complex (not equal)")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 2.0 * I };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 2.0 + 1.0 * I };
    test_assert (literal_equal_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("String == String (equal)")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("hello") };
    b = (struct literal){ .type = LT_STRING, .str = strfcstr ("hello") };
    test_assert (literal_equal_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("String == String (not equal)")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("hello") };
    b = (struct literal){ .type = LT_STRING, .str = strfcstr ("world") };
    test_assert (literal_equal_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Object + Object")
  {
    /* Array + Array */
  }

  literal_pair invalid_combos[] = {
    { LT_BOOL, LT_STRING },
    { LT_BOOL, LT_COMPLEX },
    { LT_BOOL, LT_OBJECT },
    { LT_BOOL, LT_ARRAY },
    { LT_BOOL, LT_INTEGER },
    { LT_BOOL, LT_DECIMAL },

    { LT_STRING, LT_BOOL },
    { LT_STRING, LT_COMPLEX },
    { LT_STRING, LT_OBJECT },
    { LT_STRING, LT_ARRAY },
    { LT_STRING, LT_INTEGER },
    { LT_STRING, LT_DECIMAL },

    { LT_INTEGER, LT_BOOL },
    { LT_INTEGER, LT_STRING },
    { LT_INTEGER, LT_OBJECT },
    { LT_INTEGER, LT_ARRAY },

    { LT_DECIMAL, LT_BOOL },
    { LT_DECIMAL, LT_STRING },
    { LT_DECIMAL, LT_OBJECT },
    { LT_DECIMAL, LT_ARRAY },

    { LT_COMPLEX, LT_BOOL },
    { LT_COMPLEX, LT_STRING },
    { LT_COMPLEX, LT_OBJECT },
    { LT_COMPLEX, LT_ARRAY },

    { LT_OBJECT, LT_BOOL },
    { LT_OBJECT, LT_STRING },
    { LT_OBJECT, LT_INTEGER },
    { LT_OBJECT, LT_DECIMAL },
    { LT_OBJECT, LT_COMPLEX },
    { LT_OBJECT, LT_ARRAY },

    { LT_ARRAY, LT_BOOL },
    { LT_ARRAY, LT_STRING },
    { LT_ARRAY, LT_INTEGER },
    { LT_ARRAY, LT_DECIMAL },
    { LT_ARRAY, LT_COMPLEX },
    { LT_ARRAY, LT_OBJECT },
  };

  for (u32 i = 0; i < sizeof (invalid_combos) / sizeof (invalid_combos[0]); i++)
    {
      a = (struct literal){ .type = invalid_combos[i].lhs };
      b = (struct literal){ .type = invalid_combos[i].rhs };
      err_t ret = literal_equal_equal_literal (&a, &b, &err);
      if (ret != ERR_SYNTAX)
        {
          i_log_failure ("Expected equal equal to return syntax error: %s %s\n", literal_t_tostr (a.type), literal_t_tostr (b.type));
        }
      test_assert (ret == ERR_SYNTAX);
      err.cause_code = SUCCESS;
    }
}
#endif

err_t
literal_bang_equal_literal (struct literal *dest, const struct literal *right, error *e)
{
  BOOL_NUMBER_TYPE_COERCE (dest, !=, right);

  if (dest->type == LT_STRING && right->type == LT_STRING)
    {
      dest->bl = !string_equal (dest->str, right->str);
      dest->type = LT_BOOL;
      return SUCCESS;
    }
  else if (dest->type == LT_OBJECT && right->type == LT_OBJECT)
    {
      dest->bl = !object_equal (&dest->obj, &right->obj);
      dest->type = LT_BOOL;
      return SUCCESS;
    }
  else if (dest->type == LT_ARRAY && right->type == LT_ARRAY)
    {
      dest->bl = !array_equal (&dest->arr, &right->arr);
      dest->type = LT_BOOL;
      return SUCCESS;
    }
  else if (dest->type == LT_BOOL && right->type == LT_BOOL)
    {
      dest->bl = dest->bl != right->bl;
      dest->type = LT_BOOL;
      return SUCCESS;
    }

  return ERR_UNSUPPORTED_BIN_OP (dest, right, "!=");
}

#ifndef NTEST
TEST (TT_UNIT, literal_bang_equal_literal)
{
  error err = error_create ();
  struct literal a, b;
  TEST_CASE ("TRUE != TRUE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    test_assert (literal_bang_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("TRUE != FALSE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    test_assert (literal_bang_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("FALSE != FALSE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = false };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    test_assert (literal_bang_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Integer != Integer (equal)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 5 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 5 };
    test_assert (literal_bang_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Integer != Integer (not equal)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 3 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 7 };
    test_assert (literal_bang_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Decimal != Decimal (equal)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 2.5 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 2.5 };
    test_assert (literal_bang_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Decimal != Decimal (not equal)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 2.5 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 1.0 };
    test_assert (literal_bang_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Integer != Decimal (equal)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 2.0 };
    test_assert (literal_bang_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Decimal != Integer (not equal)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 3.0 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 5 };
    test_assert (literal_bang_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Complex != Complex (equal)")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 2.0 * I };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 2.0 * I };
    test_assert (literal_bang_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Complex != Complex (not equal)")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 2.0 * I };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 2.0 + 2.0 * I };
    test_assert (literal_bang_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("String != String (equal)")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("same") };
    b = (struct literal){ .type = LT_STRING, .str = strfcstr ("same") };
    test_assert (literal_bang_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("String != String (not equal)")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("a") };
    b = (struct literal){ .type = LT_STRING, .str = strfcstr ("b") };
    test_assert (literal_bang_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Object + Object")
  {
    /* Array + Array */
  }

  literal_pair invalid_combos[] = {
    { LT_BOOL, LT_STRING },
    { LT_BOOL, LT_COMPLEX },
    { LT_BOOL, LT_OBJECT },
    { LT_BOOL, LT_ARRAY },
    { LT_BOOL, LT_INTEGER },
    { LT_BOOL, LT_DECIMAL },

    { LT_STRING, LT_BOOL },
    { LT_STRING, LT_COMPLEX },
    { LT_STRING, LT_OBJECT },
    { LT_STRING, LT_ARRAY },
    { LT_STRING, LT_INTEGER },
    { LT_STRING, LT_DECIMAL },

    { LT_INTEGER, LT_BOOL },
    { LT_INTEGER, LT_STRING },
    { LT_INTEGER, LT_OBJECT },
    { LT_INTEGER, LT_ARRAY },

    { LT_DECIMAL, LT_BOOL },
    { LT_DECIMAL, LT_STRING },
    { LT_DECIMAL, LT_OBJECT },
    { LT_DECIMAL, LT_ARRAY },

    { LT_COMPLEX, LT_BOOL },
    { LT_COMPLEX, LT_STRING },
    { LT_COMPLEX, LT_OBJECT },
    { LT_COMPLEX, LT_ARRAY },

    { LT_OBJECT, LT_BOOL },
    { LT_OBJECT, LT_STRING },
    { LT_OBJECT, LT_INTEGER },
    { LT_OBJECT, LT_DECIMAL },
    { LT_OBJECT, LT_COMPLEX },
    { LT_OBJECT, LT_ARRAY },

    { LT_ARRAY, LT_BOOL },
    { LT_ARRAY, LT_STRING },
    { LT_ARRAY, LT_INTEGER },
    { LT_ARRAY, LT_DECIMAL },
    { LT_ARRAY, LT_COMPLEX },
    { LT_ARRAY, LT_OBJECT },
  };

  for (u32 i = 0; i < sizeof (invalid_combos) / sizeof (invalid_combos[0]); i++)
    {
      a = (struct literal){ .type = invalid_combos[i].lhs };
      b = (struct literal){ .type = invalid_combos[i].rhs };
      err_t ret = literal_bang_equal_literal (&a, &b, &err);
      if (ret != ERR_SYNTAX)
        {
          i_log_failure ("Expected bang equal to return syntax error: %s %s\n", literal_t_tostr (a.type), literal_t_tostr (b.type));
        }
      test_assert (ret == ERR_SYNTAX);
      err.cause_code = SUCCESS;
    }
}
#endif

err_t
literal_greater_literal (struct literal *dest, const struct literal *right, error *e)
{
  BOOL_NUMBER_TYPE_COERCE_CABS (dest, >, right);

  if (dest->type == LT_STRING && right->type == LT_STRING)
    {
      dest->bl = string_greater_string (dest->str, right->str);
      dest->type = LT_BOOL;
      return SUCCESS;
    }

  return ERR_UNSUPPORTED_BIN_OP (dest, right, ">");
}

#ifndef NTEST
TEST (TT_UNIT, literal_greater_literal)
{
  error err = error_create ();
  struct literal a, b;
  TEST_CASE ("TRUE > FALSE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    test_assert (literal_greater_literal (&a, &b, &err) == ERR_SYNTAX);
    err.cause_code = SUCCESS;
  }

  TEST_CASE ("FALSE > TRUE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = false };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    test_assert (literal_greater_literal (&a, &b, &err) == ERR_SYNTAX);
    err.cause_code = SUCCESS;
  }

  TEST_CASE ("Integer > Integer (true)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 10 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    test_assert (literal_greater_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Integer > Integer (false)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 1 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 4 };
    test_assert (literal_greater_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Decimal > Decimal (true)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 4.5 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 2.0 };
    test_assert (literal_greater_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Decimal > Decimal (false)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 1.25 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 3.0 };
    test_assert (literal_greater_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Integer > Decimal")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 5 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 4.5 };
    test_assert (literal_greater_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Decimal > Integer")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 2.5 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 5 };
    test_assert (literal_greater_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Complex > Complex (by magnitude: true)")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 3.0 + 4.0 * I }; /* mag = 5 */
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 1.0 * I }; /* mag ≈ 1.41 */
    test_assert (literal_greater_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Complex > Complex (false)")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 1.0 * I };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 3.0 + 4.0 * I };
    test_assert (literal_greater_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("String > String (true)")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("zebra") };
    b = (struct literal){ .type = LT_STRING, .str = strfcstr ("apple") };
    test_assert (literal_greater_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("String > String (false)")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("a") };
    b = (struct literal){ .type = LT_STRING, .str = strfcstr ("z") };
    test_assert (literal_greater_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Object + Object")
  {
    /* Array + Array */
  }

  literal_pair invalid_combos[] = {
    { LT_BOOL, LT_STRING },
    { LT_BOOL, LT_COMPLEX },
    { LT_BOOL, LT_OBJECT },
    { LT_BOOL, LT_ARRAY },
    { LT_BOOL, LT_INTEGER },
    { LT_BOOL, LT_DECIMAL },

    { LT_STRING, LT_BOOL },
    { LT_STRING, LT_COMPLEX },
    { LT_STRING, LT_OBJECT },
    { LT_STRING, LT_ARRAY },
    { LT_STRING, LT_INTEGER },
    { LT_STRING, LT_DECIMAL },

    { LT_INTEGER, LT_BOOL },
    { LT_INTEGER, LT_STRING },
    { LT_INTEGER, LT_OBJECT },
    { LT_INTEGER, LT_ARRAY },

    { LT_DECIMAL, LT_BOOL },
    { LT_DECIMAL, LT_STRING },
    { LT_DECIMAL, LT_OBJECT },
    { LT_DECIMAL, LT_ARRAY },

    { LT_COMPLEX, LT_BOOL },
    { LT_COMPLEX, LT_STRING },
    { LT_COMPLEX, LT_OBJECT },
    { LT_COMPLEX, LT_ARRAY },

    { LT_OBJECT, LT_BOOL },
    { LT_OBJECT, LT_STRING },
    { LT_OBJECT, LT_INTEGER },
    { LT_OBJECT, LT_DECIMAL },
    { LT_OBJECT, LT_COMPLEX },
    { LT_OBJECT, LT_ARRAY },

    { LT_ARRAY, LT_BOOL },
    { LT_ARRAY, LT_STRING },
    { LT_ARRAY, LT_INTEGER },
    { LT_ARRAY, LT_DECIMAL },
    { LT_ARRAY, LT_COMPLEX },
    { LT_ARRAY, LT_OBJECT },
  };

  for (u32 i = 0; i < sizeof (invalid_combos) / sizeof (invalid_combos[0]); i++)
    {
      a = (struct literal){ .type = invalid_combos[i].lhs };
      b = (struct literal){ .type = invalid_combos[i].rhs };
      err_t ret = literal_greater_literal (&a, &b, &err);
      if (ret != ERR_SYNTAX)
        {
          i_log_failure ("Expected greater to return syntax error: %s %s\n", literal_t_tostr (a.type), literal_t_tostr (b.type));
        }
      test_assert (ret == ERR_SYNTAX);
      err.cause_code = SUCCESS;
    }
}
#endif

err_t
literal_greater_equal_literal (struct literal *dest, const struct literal *right, error *e)
{
  BOOL_NUMBER_TYPE_COERCE_CABS (dest, >=, right);

  if (dest->type == LT_STRING && right->type == LT_STRING)
    {
      dest->bl = string_greater_equal_string (dest->str, right->str);
      dest->type = LT_BOOL;
      return SUCCESS;
    }

  return ERR_UNSUPPORTED_BIN_OP (dest, right, ">=");
}

#ifndef NTEST
TEST (TT_UNIT, literal_greater_equal_literal)
{
  error err = error_create ();
  struct literal a, b;
  TEST_CASE ("TRUE >= TRUE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == ERR_SYNTAX);
    err.cause_code = SUCCESS;
  }

  TEST_CASE ("TRUE >= FALSE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == ERR_SYNTAX);
    err.cause_code = SUCCESS;
  }

  TEST_CASE ("FALSE >= TRUE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = false };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == ERR_SYNTAX);
    err.cause_code = SUCCESS;
  }

  TEST_CASE ("Integer >= Integer (equal)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 5 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 5 };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Integer >= Integer (true)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 10 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 4 };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Integer >= Integer (false)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 7 };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Decimal >= Decimal (equal)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 5.0 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 5.0 };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Decimal >= Decimal (true)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 8.5 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 4.1 };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Decimal >= Decimal (false)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 3.14 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 9.8 };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Complex >= Complex (equal)")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 3.0 + 4.0 * I };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 3.0 + 4.0 * I };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Complex >= Complex (true)")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 5.0 + 0.0 * I };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 3.0 + 0.0 * I };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Complex >= Complex (false)")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 1.0 * I };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 4.0 + 3.0 * I };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("String >= String (equal)")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("test") };
    b = (struct literal){ .type = LT_STRING, .str = strfcstr ("test") };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("String >= String (true)")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("z") };
    b = (struct literal){ .type = LT_STRING, .str = strfcstr ("a") };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("String >= String (false)")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("a") };
    b = (struct literal){ .type = LT_STRING, .str = strfcstr ("b") };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }
}
#endif

err_t
literal_less_literal (struct literal *dest, const struct literal *right, error *e)
{
  BOOL_NUMBER_TYPE_COERCE_CABS (dest, <, right);

  if (dest->type == LT_STRING && right->type == LT_STRING)
    {
      dest->bl = string_less_string (dest->str, right->str);
      dest->type = LT_BOOL;
      return SUCCESS;
    }

  return ERR_UNSUPPORTED_BIN_OP (dest, right, "<");
}

#ifndef NTEST
TEST (TT_UNIT, literal_less_literal)
{
  error err = error_create ();
  struct literal a, b;
  TEST_CASE ("TRUE >= TRUE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == ERR_SYNTAX);
    err.cause_code = SUCCESS;
  }

  TEST_CASE ("TRUE >= FALSE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == ERR_SYNTAX);
    err.cause_code = SUCCESS;
  }

  TEST_CASE ("FALSE >= TRUE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = false };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == ERR_SYNTAX);
    err.cause_code = SUCCESS;
  }

  TEST_CASE ("Integer >= Integer (equal)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 5 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 5 };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Integer >= Integer (true)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 10 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 4 };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Integer >= Integer (false)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 7 };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Decimal >= Decimal (equal)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 2.5 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 2.5 };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Decimal >= Decimal (true)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 3.5 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 2.0 };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Decimal >= Decimal (false)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 1.0 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 2.0 };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Integer >= Decimal")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 5 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 4.5 };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Decimal >= Integer")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 2.0 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Complex >= Complex (equal)")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 3.0 + 4.0 * I };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 3.0 + 4.0 * I };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Complex >= Complex (true)")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 5.0 + 0.0 * I };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 3.0 + 0.0 * I };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Complex >= Complex (false)")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 1.0 * I };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 4.0 + 3.0 * I };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("String >= String (equal)")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("test") };
    b = (struct literal){ .type = LT_STRING, .str = strfcstr ("test") };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("String >= String (true)")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("z") };
    b = (struct literal){ .type = LT_STRING, .str = strfcstr ("a") };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("String >= String (false)")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("a") };
    b = (struct literal){ .type = LT_STRING, .str = strfcstr ("b") };
    test_assert (literal_greater_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Object + Object")
  {
    /* Array + Array */
  }

  literal_pair invalid_combos[] = {
    { LT_BOOL, LT_STRING },
    { LT_BOOL, LT_COMPLEX },
    { LT_BOOL, LT_OBJECT },
    { LT_BOOL, LT_ARRAY },
    { LT_BOOL, LT_INTEGER },
    { LT_BOOL, LT_DECIMAL },

    { LT_STRING, LT_BOOL },
    { LT_STRING, LT_COMPLEX },
    { LT_STRING, LT_OBJECT },
    { LT_STRING, LT_ARRAY },
    { LT_STRING, LT_INTEGER },
    { LT_STRING, LT_DECIMAL },

    { LT_INTEGER, LT_BOOL },
    { LT_INTEGER, LT_STRING },
    { LT_INTEGER, LT_OBJECT },
    { LT_INTEGER, LT_ARRAY },

    { LT_DECIMAL, LT_BOOL },
    { LT_DECIMAL, LT_STRING },
    { LT_DECIMAL, LT_OBJECT },
    { LT_DECIMAL, LT_ARRAY },

    { LT_COMPLEX, LT_BOOL },
    { LT_COMPLEX, LT_STRING },
    { LT_COMPLEX, LT_OBJECT },
    { LT_COMPLEX, LT_ARRAY },

    { LT_OBJECT, LT_BOOL },
    { LT_OBJECT, LT_STRING },
    { LT_OBJECT, LT_INTEGER },
    { LT_OBJECT, LT_DECIMAL },
    { LT_OBJECT, LT_COMPLEX },
    { LT_OBJECT, LT_ARRAY },

    { LT_ARRAY, LT_BOOL },
    { LT_ARRAY, LT_STRING },
    { LT_ARRAY, LT_INTEGER },
    { LT_ARRAY, LT_DECIMAL },
    { LT_ARRAY, LT_COMPLEX },
    { LT_ARRAY, LT_OBJECT },
  };

  for (u32 i = 0; i < sizeof (invalid_combos) / sizeof (invalid_combos[0]); i++)
    {
      a = (struct literal){ .type = invalid_combos[i].lhs };
      b = (struct literal){ .type = invalid_combos[i].rhs };
      err_t ret = literal_greater_equal_literal (&a, &b, &err);
      if (ret != ERR_SYNTAX)
        {
          i_log_failure ("Expected greater equal to return syntax error: %s %s\n", literal_t_tostr (a.type), literal_t_tostr (b.type));
        }
      test_assert (ret == ERR_SYNTAX);
      err.cause_code = SUCCESS;
    }
}
#endif

err_t
literal_less_equal_literal (struct literal *dest, const struct literal *right, error *e)
{
  BOOL_NUMBER_TYPE_COERCE_CABS (dest, <=, right);

  if (dest->type == LT_STRING && right->type == LT_STRING)
    {
      dest->bl = string_less_equal_string (dest->str, right->str);
      dest->type = LT_BOOL;
      return SUCCESS;
    }

  return ERR_UNSUPPORTED_BIN_OP (dest, right, "<=");
}

#ifndef NTEST
TEST (TT_UNIT, literal_less_equal_literal)
{
  error err = error_create ();
  struct literal a, b;
  TEST_CASE ("TRUE <= TRUE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    test_assert (literal_less_equal_literal (&a, &b, &err) == ERR_SYNTAX);
    err.cause_code = SUCCESS;
  }

  TEST_CASE ("TRUE <= FALSE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    test_assert (literal_less_equal_literal (&a, &b, &err) == ERR_SYNTAX);
    err.cause_code = SUCCESS;
  }

  TEST_CASE ("FALSE <= TRUE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = false };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    test_assert (literal_less_equal_literal (&a, &b, &err) == ERR_SYNTAX);
    err.cause_code = SUCCESS;
  }

  TEST_CASE ("Integer <= Integer (equal)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 5 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 5 };
    test_assert (literal_less_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Integer <= Integer (true)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 3 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 7 };
    test_assert (literal_less_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Integer <= Integer (false)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 9 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 4 };
    test_assert (literal_less_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Decimal <= Decimal (equal)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 1.5 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 1.5 };
    test_assert (literal_less_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Decimal <= Decimal (true)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 1.25 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 3.0 };
    test_assert (literal_less_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Decimal <= Decimal (false)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 3.0 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 1.0 };
    test_assert (literal_less_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Integer <= Decimal")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 2.5 };
    test_assert (literal_less_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Decimal <= Integer")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 4.5 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 4 };
    test_assert (literal_less_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Complex <= Complex (equal)")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 3.0 + 4.0 * I };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 3.0 + 4.0 * I };
    test_assert (literal_less_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Complex <= Complex (true)")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 1.0 * I };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 3.0 + 4.0 * I };
    test_assert (literal_less_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Complex <= Complex (false)")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 5.0 + 0.0 * I };
    b = (struct literal){ .type = LT_COMPLEX, .cplx = 1.0 + 1.0 * I };
    test_assert (literal_less_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("String <= String (equal)")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("abc") };
    b = (struct literal){ .type = LT_STRING, .str = strfcstr ("abc") };
    test_assert (literal_less_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("String <= String (true)")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("abc") };
    b = (struct literal){ .type = LT_STRING, .str = strfcstr ("bcd") };
    test_assert (literal_less_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("String <= String (false)")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("xyz") };
    b = (struct literal){ .type = LT_STRING, .str = strfcstr ("abc") };
    test_assert (literal_less_equal_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Object + Object")
  {
    /* Array + Array */
  }

  literal_pair invalid_combos[] = {
    { LT_BOOL, LT_STRING },
    { LT_BOOL, LT_COMPLEX },
    { LT_BOOL, LT_OBJECT },
    { LT_BOOL, LT_ARRAY },
    { LT_BOOL, LT_INTEGER },
    { LT_BOOL, LT_DECIMAL },

    { LT_STRING, LT_BOOL },
    { LT_STRING, LT_COMPLEX },
    { LT_STRING, LT_OBJECT },
    { LT_STRING, LT_ARRAY },
    { LT_STRING, LT_INTEGER },
    { LT_STRING, LT_DECIMAL },

    { LT_INTEGER, LT_BOOL },
    { LT_INTEGER, LT_STRING },
    { LT_INTEGER, LT_OBJECT },
    { LT_INTEGER, LT_ARRAY },

    { LT_DECIMAL, LT_BOOL },
    { LT_DECIMAL, LT_STRING },
    { LT_DECIMAL, LT_OBJECT },
    { LT_DECIMAL, LT_ARRAY },

    { LT_COMPLEX, LT_BOOL },
    { LT_COMPLEX, LT_STRING },
    { LT_COMPLEX, LT_OBJECT },
    { LT_COMPLEX, LT_ARRAY },

    { LT_OBJECT, LT_BOOL },
    { LT_OBJECT, LT_STRING },
    { LT_OBJECT, LT_INTEGER },
    { LT_OBJECT, LT_DECIMAL },
    { LT_OBJECT, LT_COMPLEX },
    { LT_OBJECT, LT_ARRAY },

    { LT_ARRAY, LT_BOOL },
    { LT_ARRAY, LT_STRING },
    { LT_ARRAY, LT_INTEGER },
    { LT_ARRAY, LT_DECIMAL },
    { LT_ARRAY, LT_COMPLEX },
    { LT_ARRAY, LT_OBJECT },
  };

  for (u32 i = 0; i < sizeof (invalid_combos) / sizeof (invalid_combos[0]); i++)
    {
      a = (struct literal){ .type = invalid_combos[i].lhs };
      b = (struct literal){ .type = invalid_combos[i].rhs };
      err_t ret = literal_less_equal_literal (&a, &b, &err);
      if (ret != ERR_SYNTAX)
        {
          i_log_failure ("Expected less equal to return syntax error: %s %s\n", literal_t_tostr (a.type), literal_t_tostr (b.type));
        }
      test_assert (ret == ERR_SYNTAX);
      err.cause_code = SUCCESS;
    }
}
#endif

/* dest = dest ^ right */
err_t
literal_caret_literal (struct literal *dest, const struct literal *right, error *e)
{
  BITMANIP_TYPE_COERCE (dest, ^, right);

  return ERR_UNSUPPORTED_BIN_OP (dest, right, "^");
}

#ifndef NTEST
TEST (TT_UNIT, literal_caret_literal)
{
  error err = error_create ();
  struct literal a, b;
  TEST_CASE ("true ^ true -> false")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    test_assert (literal_caret_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("true ^ false -> true")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    test_assert (literal_caret_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("false ^ true -> true")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = false };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    test_assert (literal_caret_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("int ^ int")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 6 }; /* 110 */
    b = (struct literal){ .type = LT_INTEGER, .integer = 3 }; /* 011 */
    test_assert (literal_caret_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == 5); /* 101 */
  }

  TEST_CASE ("int ^ bool")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 6 };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    test_assert (literal_caret_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == 7);
  }

  TEST_CASE ("bool ^ int")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    test_assert (literal_caret_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == 3);
  }

  literal_pair invalid_combos[] = {
    { LT_BOOL, LT_STRING },
    { LT_BOOL, LT_DECIMAL },
    { LT_BOOL, LT_COMPLEX },
    { LT_BOOL, LT_OBJECT },
    { LT_BOOL, LT_ARRAY },

    { LT_STRING, LT_BOOL },
    { LT_STRING, LT_INTEGER },
    { LT_STRING, LT_DECIMAL },
    { LT_STRING, LT_COMPLEX },
    { LT_STRING, LT_STRING },
    { LT_STRING, LT_OBJECT },
    { LT_STRING, LT_ARRAY },

    { LT_INTEGER, LT_STRING },
    { LT_INTEGER, LT_DECIMAL },
    { LT_INTEGER, LT_COMPLEX },
    { LT_INTEGER, LT_OBJECT },
    { LT_INTEGER, LT_ARRAY },

    { LT_DECIMAL, LT_BOOL },
    { LT_DECIMAL, LT_INTEGER },
    { LT_DECIMAL, LT_DECIMAL },
    { LT_DECIMAL, LT_COMPLEX },
    { LT_DECIMAL, LT_STRING },
    { LT_DECIMAL, LT_OBJECT },
    { LT_DECIMAL, LT_ARRAY },

    { LT_COMPLEX, LT_BOOL },
    { LT_COMPLEX, LT_INTEGER },
    { LT_COMPLEX, LT_DECIMAL },
    { LT_COMPLEX, LT_COMPLEX },
    { LT_COMPLEX, LT_STRING },
    { LT_COMPLEX, LT_OBJECT },
    { LT_COMPLEX, LT_ARRAY },

    { LT_OBJECT, LT_BOOL },
    { LT_OBJECT, LT_INTEGER },
    { LT_OBJECT, LT_DECIMAL },
    { LT_OBJECT, LT_COMPLEX },
    { LT_OBJECT, LT_STRING },
    { LT_OBJECT, LT_OBJECT },
    { LT_OBJECT, LT_ARRAY },

    { LT_ARRAY, LT_BOOL },
    { LT_ARRAY, LT_INTEGER },
    { LT_ARRAY, LT_DECIMAL },
    { LT_ARRAY, LT_COMPLEX },
    { LT_ARRAY, LT_STRING },
    { LT_ARRAY, LT_OBJECT },
    { LT_ARRAY, LT_ARRAY },
  };

  for (u32 i = 0; i < sizeof (invalid_combos) / sizeof (invalid_combos[0]); i++)
    {
      a = (struct literal){ .type = invalid_combos[i].lhs };
      b = (struct literal){ .type = invalid_combos[i].rhs };
      err_t ret = literal_caret_literal (&a, &b, &err);
      if (ret != ERR_SYNTAX)
        {
          i_log_failure ("Expected caret to return syntax error: %s %s\n", literal_t_tostr (a.type), literal_t_tostr (b.type));
        }
      test_assert (ret == ERR_SYNTAX);
      err.cause_code = SUCCESS;
    }
}
#endif

/* dest = dest % right */
err_t
literal_mod_literal (struct literal *dest, const struct literal *right, error *e)
{
  /* int % int */
  if (dest->type == LT_INTEGER && right->type == LT_INTEGER)
    {
      dest->integer = py_mod_i32 (dest->integer, right->integer);
      return SUCCESS;
    }

  /* dec % dec */
  else if (dest->type == LT_DECIMAL && right->type == LT_DECIMAL)
    {
      dest->decimal = py_mod_f32 (dest->decimal, right->decimal);
      return SUCCESS;
    }

  /* dec % int */
  else if (dest->type == LT_DECIMAL && right->type == LT_INTEGER)
    {
      dest->decimal = py_mod_f32 (dest->decimal, (float)right->integer);
      return SUCCESS;
    }

  /* int % dec */
  else if (dest->type == LT_INTEGER && right->type == LT_DECIMAL)
    {
      dest->decimal = py_mod_f32 ((f32)dest->integer, right->decimal);
      dest->type = LT_DECIMAL;
      return SUCCESS;
    }

  return ERR_UNSUPPORTED_BIN_OP (dest, right, "%");
}

#ifndef NTEST
TEST (TT_UNIT, literal_mod_literal)
{
  error err = error_create ();
  struct literal a, b;
  TEST_CASE ("Integer %% Integer")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 10 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 3 };
    test_assert (literal_mod_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == 1);
  }

  TEST_CASE ("Decimal %% Decimal")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 5.5f };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 2.0f };
    test_assert (literal_mod_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_DECIMAL);
    test_assert (a.decimal == 1.5f);
  }

  TEST_CASE ("Decimal %% Integer")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 5.5f };
    b = (struct literal){ .type = LT_INTEGER, .integer = 2 };
    test_assert (literal_mod_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_DECIMAL);
    test_assert (a.decimal == 1.5f);
  }

  TEST_CASE ("Integer %% Decimal")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 5 };
    b = (struct literal){ .type = LT_DECIMAL, .decimal = 2.0f };
    test_assert (literal_mod_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_DECIMAL);
    test_assert (a.decimal == 1.0f);
  }

  /* Invalid combos */
  literal_pair invalid_combos[] = {
    { LT_BOOL, LT_INTEGER },
    { LT_BOOL, LT_DECIMAL },
    { LT_BOOL, LT_COMPLEX },
    { LT_BOOL, LT_STRING },
    { LT_BOOL, LT_OBJECT },
    { LT_BOOL, LT_ARRAY },

    { LT_STRING, LT_BOOL },
    { LT_STRING, LT_INTEGER },
    { LT_STRING, LT_DECIMAL },
    { LT_STRING, LT_COMPLEX },
    { LT_STRING, LT_STRING },
    { LT_STRING, LT_OBJECT },
    { LT_STRING, LT_ARRAY },

    { LT_INTEGER, LT_BOOL },
    { LT_INTEGER, LT_STRING },
    { LT_INTEGER, LT_COMPLEX },
    { LT_INTEGER, LT_OBJECT },
    { LT_INTEGER, LT_ARRAY },

    { LT_DECIMAL, LT_BOOL },
    { LT_DECIMAL, LT_STRING },
    { LT_DECIMAL, LT_COMPLEX },
    { LT_DECIMAL, LT_OBJECT },
    { LT_DECIMAL, LT_ARRAY },

    { LT_COMPLEX, LT_BOOL },
    { LT_COMPLEX, LT_STRING },
    { LT_COMPLEX, LT_INTEGER },
    { LT_COMPLEX, LT_DECIMAL },
    { LT_COMPLEX, LT_OBJECT },
    { LT_COMPLEX, LT_ARRAY },

    { LT_OBJECT, LT_BOOL },
    { LT_OBJECT, LT_STRING },
    { LT_OBJECT, LT_INTEGER },
    { LT_OBJECT, LT_DECIMAL },
    { LT_OBJECT, LT_COMPLEX },
    { LT_OBJECT, LT_ARRAY },

    { LT_ARRAY, LT_BOOL },
    { LT_ARRAY, LT_STRING },
    { LT_ARRAY, LT_INTEGER },
    { LT_ARRAY, LT_DECIMAL },
    { LT_ARRAY, LT_COMPLEX },
    { LT_ARRAY, LT_OBJECT },
  };

  for (u32 i = 0; i < sizeof (invalid_combos) / sizeof (invalid_combos[0]); i++)
    {
      a = (struct literal){ .type = invalid_combos[i].lhs };
      b = (struct literal){ .type = invalid_combos[i].rhs };
      err_t ret = literal_mod_literal (&a, &b, &err);
      if (ret != ERR_SYNTAX)
        {
          i_log_failure ("Expected mod to return syntax error: %s %s\n", literal_t_tostr (a.type), literal_t_tostr (b.type));
        }
      test_assert (ret == ERR_SYNTAX);
      err.cause_code = SUCCESS;
    }
}
#endif

/* dest = dest | right */
err_t
literal_pipe_literal (struct literal *dest, const struct literal *right, error *e)
{
  BITMANIP_TYPE_COERCE (dest, |, right);

  return ERR_UNSUPPORTED_BIN_OP (dest, right, "|");
}

#ifndef NTEST
TEST (TT_UNIT, literal_pipe_literal)
{
  error err = error_create ();
  struct literal a, b;
  TEST_CASE ("TRUE | TRUE -> TRUE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    test_assert (literal_pipe_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("TRUE | FALSE -> TRUE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    test_assert (literal_pipe_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("FALSE | FALSE -> FALSE")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = false };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    test_assert (literal_pipe_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("INT | INT -> INT")
  {
    a.type = LT_INTEGER;
    a.integer = 10; /* 0b1010 */
    b.type = LT_INTEGER;
    b.integer = 12; /* 0b1100 */
    test_assert (literal_pipe_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == 14); /* 0b1110 */
  }

  TEST_CASE ("INT | BOOL -> INT")
  {
    a.type = LT_INTEGER;
    a.integer = 8; /* 0b1000 */
    b.type = LT_BOOL;
    b.bl = true;
    test_assert (literal_pipe_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == 9); /* 0b1001 */
  }

  TEST_CASE ("BOOL | INT -> INT")
  {
    a.type = LT_BOOL;
    a.bl = true;
    b.type = LT_INTEGER;
    b.integer = 4; /* 0b0100 */
    test_assert (literal_pipe_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == 5); /* 0b0101 */
  }

  literal_pair invalid_combos[] = {
    { LT_BOOL, LT_STRING },
    { LT_BOOL, LT_DECIMAL },
    { LT_BOOL, LT_COMPLEX },
    { LT_BOOL, LT_OBJECT },
    { LT_BOOL, LT_ARRAY },

    { LT_STRING, LT_BOOL },
    { LT_STRING, LT_INTEGER },
    { LT_STRING, LT_DECIMAL },
    { LT_STRING, LT_COMPLEX },
    { LT_STRING, LT_STRING },
    { LT_STRING, LT_OBJECT },
    { LT_STRING, LT_ARRAY },

    { LT_INTEGER, LT_STRING },
    { LT_INTEGER, LT_DECIMAL },
    { LT_INTEGER, LT_COMPLEX },
    { LT_INTEGER, LT_OBJECT },
    { LT_INTEGER, LT_ARRAY },

    { LT_DECIMAL, LT_BOOL },
    { LT_DECIMAL, LT_INTEGER },
    { LT_DECIMAL, LT_DECIMAL },
    { LT_DECIMAL, LT_COMPLEX },
    { LT_DECIMAL, LT_STRING },
    { LT_DECIMAL, LT_OBJECT },
    { LT_DECIMAL, LT_ARRAY },

    { LT_COMPLEX, LT_BOOL },
    { LT_COMPLEX, LT_INTEGER },
    { LT_COMPLEX, LT_DECIMAL },
    { LT_COMPLEX, LT_COMPLEX },
    { LT_COMPLEX, LT_STRING },
    { LT_COMPLEX, LT_OBJECT },
    { LT_COMPLEX, LT_ARRAY },

    { LT_OBJECT, LT_BOOL },
    { LT_OBJECT, LT_INTEGER },
    { LT_OBJECT, LT_DECIMAL },
    { LT_OBJECT, LT_COMPLEX },
    { LT_OBJECT, LT_STRING },
    { LT_OBJECT, LT_OBJECT },
    { LT_OBJECT, LT_ARRAY },

    { LT_ARRAY, LT_BOOL },
    { LT_ARRAY, LT_INTEGER },
    { LT_ARRAY, LT_DECIMAL },
    { LT_ARRAY, LT_COMPLEX },
    { LT_ARRAY, LT_STRING },
    { LT_ARRAY, LT_OBJECT },
    { LT_ARRAY, LT_ARRAY },
  };

  for (u32 i = 0; i < sizeof (invalid_combos) / sizeof (invalid_combos[0]); i++)
    {
      a = (struct literal){ .type = invalid_combos[i].lhs };
      b = (struct literal){ .type = invalid_combos[i].rhs };
      err_t ret = literal_pipe_literal (&a, &b, &err);
      if (ret != ERR_SYNTAX)
        {
          i_log_failure ("Expected pipe to return syntax error: %s %s\n", literal_t_tostr (a.type), literal_t_tostr (b.type));
        }
      test_assert (ret == ERR_SYNTAX);
      err.cause_code = SUCCESS;
    }
}
#endif

/* dest = dest || right */
void
literal_pipe_pipe_literal (struct literal *dest, const struct literal *right)
{
  bool bleft = literal_truthy (dest);
  bool bright = literal_truthy (right);

  dest->bl = bleft || bright;
  dest->type = LT_BOOL;
}

#ifndef NTEST
TEST (TT_UNIT, literal_pipe_pipe_literal)
{
  struct literal a, b;
  TEST_CASE ("true || true -> true")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    literal_pipe_pipe_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("true || false -> true")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    literal_pipe_pipe_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("false || true -> true")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = false };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    literal_pipe_pipe_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("false || false -> false")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = false };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    literal_pipe_pipe_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("integer (0) || false -> false")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 0 };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    literal_pipe_pipe_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("integer (3) || false -> true")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 3 };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    literal_pipe_pipe_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("decimal (0.0) || false -> false")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 0.0 };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    literal_pipe_pipe_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("decimal (1.5) || false -> true")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 1.5 };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    literal_pipe_pipe_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("complex (0+0i) || false -> false")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 0.0 + 0.0 * I };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    literal_pipe_pipe_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("complex (2+3i) || false -> true")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 2.0 + 3.0 * I };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    literal_pipe_pipe_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("string ("
             ") || false -> false")
  {
    a = (struct literal){ .type = LT_STRING, .str = { .data = "", .len = 0 } };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    literal_pipe_pipe_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("string ( hello ) || false -> true")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("hello") };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    literal_pipe_pipe_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("array (empty) || false -> false")
  {
    a = (struct literal){ .type = LT_ARRAY, .arr = { .len = 0 } };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    literal_pipe_pipe_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("array (non-empty) || false -> true")
  {
    a = (struct literal){ .type = LT_ARRAY, .arr = { .len = 1 } };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    literal_pipe_pipe_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("object (empty) || false -> false")
  {
    a = (struct literal){ .type = LT_OBJECT, .obj = { .len = 0 } };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    literal_pipe_pipe_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("object (non-empty) || false -> true")
  {
    a = (struct literal){ .type = LT_OBJECT, .obj = { .len = 1 } };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    literal_pipe_pipe_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }
}
#endif

/* dest = dest & right */
err_t
literal_ampersand_literal (struct literal *dest, const struct literal *right, error *e)
{
  BITMANIP_TYPE_COERCE (dest, &, right);

  return ERR_UNSUPPORTED_BIN_OP (dest, right, "&");
}

#ifndef NTEST
TEST (TT_UNIT, literal_ampersand_literal)
{
  error err = error_create ();
  struct literal a, b;
  TEST_CASE ("BOOL & BOOL")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    test_assert (literal_ampersand_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == (true & false));
  }

  TEST_CASE ("INT & BOOL")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 6 };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    test_assert (literal_ampersand_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == (6 & 1));
  }

  TEST_CASE ("BOOL & INT")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_INTEGER, .integer = 3 };
    test_assert (literal_ampersand_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == (1 & 3));
  }

  TEST_CASE ("INT & INT")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 12 };
    b = (struct literal){ .type = LT_INTEGER, .integer = 10 };
    test_assert (literal_ampersand_literal (&a, &b, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == (12 & 10));
  }

  /* Invalid combinations */
  literal_pair invalid_combos[] = {
    { LT_BOOL, LT_STRING },
    { LT_BOOL, LT_COMPLEX },
    { LT_BOOL, LT_OBJECT },
    { LT_BOOL, LT_ARRAY },
    { LT_BOOL, LT_DECIMAL },

    { LT_STRING, LT_BOOL },
    { LT_STRING, LT_INTEGER },
    { LT_STRING, LT_DECIMAL },

    { LT_INTEGER, LT_STRING },
    { LT_INTEGER, LT_COMPLEX },
    { LT_INTEGER, LT_OBJECT },
    { LT_INTEGER, LT_ARRAY },
    { LT_INTEGER, LT_DECIMAL },

    { LT_COMPLEX, LT_INTEGER },
    { LT_COMPLEX, LT_BOOL },
    { LT_COMPLEX, LT_STRING },

    { LT_DECIMAL, LT_BOOL },
    { LT_DECIMAL, LT_STRING },
    { LT_DECIMAL, LT_INTEGER },
    { LT_DECIMAL, LT_COMPLEX },

    { LT_OBJECT, LT_OBJECT },
    { LT_ARRAY, LT_ARRAY },
  };

  for (u32 i = 0; i < sizeof (invalid_combos) / sizeof (invalid_combos[0]); i++)
    {
      a = (struct literal){ .type = invalid_combos[i].lhs };
      b = (struct literal){ .type = invalid_combos[i].rhs };
      err_t ret = literal_ampersand_literal (&a, &b, &err);
      if (ret != ERR_SYNTAX)
        {
          i_log_failure ("Expected ampersand to return syntax error: %s %s\n", literal_t_tostr (a.type), literal_t_tostr (b.type));
        }
      test_assert (ret == ERR_SYNTAX);
      err.cause_code = SUCCESS;
    }
}
#endif

/* dest = dest && right */
void
literal_ampersand_ampersand_literal (struct literal *dest, const struct literal *right)
{
  bool bleft = literal_truthy (dest);
  bool bright = literal_truthy (right);

  dest->bl = bleft && bright;
  dest->type = LT_BOOL;
}

#ifndef NTEST
TEST (TT_UNIT, literal_ampersand_ampersand_literal)
{
  struct literal a, b;
  TEST_CASE ("true && true -> true")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    literal_ampersand_ampersand_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("true && false -> false")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    literal_ampersand_ampersand_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("false && true -> false")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = false };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    literal_ampersand_ampersand_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("false && false -> false")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = false };
    b = (struct literal){ .type = LT_BOOL, .bl = false };
    literal_ampersand_ampersand_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("integer (0) && true -> false")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 0 };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    literal_ampersand_ampersand_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("integer (5) && true -> true")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 5 };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    literal_ampersand_ampersand_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("decimal (0.0) && true -> false")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 0.0 };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    literal_ampersand_ampersand_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("decimal (1.5) && true -> true")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 1.5 };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    literal_ampersand_ampersand_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("complex (0+0i) && true -> false")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 0.0 + 0.0 * I };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    literal_ampersand_ampersand_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("complex (2+3i) && true -> true")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 2.0 + 3.0 * I };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    literal_ampersand_ampersand_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("string ("
             ") && true -> false")
  {
    a = (struct literal){ .type = LT_STRING, .str = { .data = "", .len = 0 } };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    literal_ampersand_ampersand_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("string (hello) && true -> true")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("hello") };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    literal_ampersand_ampersand_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("array (empty) && true -> false")
  {
    a = (struct literal){ .type = LT_ARRAY, .arr = { .len = 0 } };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    literal_ampersand_ampersand_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("array (non-empty) && true -> true")
  {
    a = (struct literal){ .type = LT_ARRAY, .arr = { .len = 1 } };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    literal_ampersand_ampersand_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("object (empty) && true -> false")
  {
    a = (struct literal){ .type = LT_OBJECT, .obj = { .len = 0 } };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    literal_ampersand_ampersand_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("object (non-empty) && true -> true")
  {
    a = (struct literal){ .type = LT_OBJECT, .obj = { .len = 1 } };
    b = (struct literal){ .type = LT_BOOL, .bl = true };
    literal_ampersand_ampersand_literal (&a, &b);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }
}
#endif

/* dest = ~dest */
err_t
literal_not (struct literal *dest, error *e)
{
  if (dest->type == LT_INTEGER)
    {
      dest->integer = ~dest->integer;
      return SUCCESS;
    }
  else if (dest->type == LT_BOOL)
    {
      dest->integer = ~(dest->bl ? 1 : 0);
      dest->type = LT_INTEGER;
      return SUCCESS;
    }
  return ERR_UNSUPPORTED_UN_OP (dest, "~");
}

#ifndef NTEST
TEST (TT_UNIT, literal_not)
{
  error err = error_create ();
  struct literal a;

  TEST_CASE ("INT ~ 0 -> -1")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 0 };
    test_assert (literal_not (&a, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == -1);
  }

  TEST_CASE ("INT ~ -1 -> 0")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = -1 };
    test_assert (literal_not (&a, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == 0);
  }

  TEST_CASE ("BOOL ~true -> -2")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    test_assert (literal_not (&a, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == -2);
  }

  TEST_CASE ("BOOL ~false -> -1")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = false };
    test_assert (literal_not (&a, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == -1);
  }

  struct literal invalids[] = {
    { .type = LT_DECIMAL },
    { .type = LT_COMPLEX },
    { .type = LT_STRING },
    { .type = LT_ARRAY },
    { .type = LT_OBJECT },
  };

  for (u32 i = 0; i < sizeof (invalids) / sizeof (invalids[0]); i++)
    {
      a = invalids[i];
      err_t ret = literal_not (&a, &err);
      if (ret != ERR_SYNTAX)
        {
          i_log_failure ("Expected not to return syntax error: %s\n", literal_t_tostr (a.type));
        }
      test_assert (ret == ERR_SYNTAX);
      err.cause_code = SUCCESS;
    }
}
#endif

err_t
literal_minus (struct literal *dest, error *e)
{
  switch (dest->type)
    {
    case LT_COMPLEX:
      {
        dest->cplx = -dest->cplx;
        return SUCCESS;
      }
    case LT_INTEGER:
      {
        dest->integer = -dest->integer;
        return SUCCESS;
      }
    case LT_DECIMAL:
      {
        dest->decimal = -dest->decimal;
        return SUCCESS;
      }
    default:
      {
        break;
      }
    }

  return ERR_UNSUPPORTED_UN_OP (dest, "-");
}

#ifndef NTEST
TEST (TT_UNIT, literal_minus)
{
  error err = error_create ();

  struct literal a;

  TEST_CASE ("-Integer")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 5 };
    test_assert (literal_minus (&a, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == -5);
  }

  TEST_CASE ("-(-Integer)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = -3 };
    test_assert (literal_minus (&a, &err) == SUCCESS);
    test_assert (a.type == LT_INTEGER);
    test_assert (a.integer == 3);
  }

  TEST_CASE ("-Decimal")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 2.5 };
    test_assert (literal_minus (&a, &err) == SUCCESS);
    test_assert (a.type == LT_DECIMAL);
    test_assert (a.decimal == -2.5);
  }

  TEST_CASE ("-(-Decimal)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = -1.25 };
    test_assert (literal_minus (&a, &err) == SUCCESS);
    test_assert (a.type == LT_DECIMAL);
    test_assert (a.decimal == 1.25);
  }

  TEST_CASE ("-Complex")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 2.0 + 3.0 * I };
    test_assert (literal_minus (&a, &err) == SUCCESS);
    test_assert (a.type == LT_COMPLEX);
    test_assert (i_creal_64 (a.cplx) == -2.0);
    test_assert (i_cimag_64 (a.cplx) == -3.0);
  }

  TEST_CASE ("-True (should fail)")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    test_assert (literal_minus (&a, &err) == ERR_SYNTAX);
    err.cause_code = SUCCESS;
  }

  TEST_CASE ("-False (should fail)")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = false };
    test_assert (literal_minus (&a, &err) == ERR_SYNTAX);
    err.cause_code = SUCCESS;
  }

  TEST_CASE ("-String (should fail)")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("bad") };
    test_assert (literal_minus (&a, &err) == ERR_SYNTAX);
    err.cause_code = SUCCESS;
  }

  TEST_CASE ("-Array (should fail)")
  {
    a = (struct literal){ .type = LT_ARRAY, .arr = { .len = 0 } };
    test_assert (literal_minus (&a, &err) == ERR_SYNTAX);
    err.cause_code = SUCCESS;
  }

  TEST_CASE ("-Object (should fail)")
  {
    a = (struct literal){ .type = LT_OBJECT, .obj = { .len = 0 } };
    test_assert (literal_minus (&a, &err) == ERR_SYNTAX);
    err.cause_code = SUCCESS;
  }
}
#endif

void
literal_bang (struct literal *dest)
{
  dest->bl = !literal_truthy (dest);
  dest->type = LT_BOOL;
}

#ifndef NTEST
TEST (TT_UNIT, literal_bang)
{
  struct literal a;

  TEST_CASE ("Integer (non-zero -> false)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 42 };
    literal_bang (&a);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Integer (zero -> true)")
  {
    a = (struct literal){ .type = LT_INTEGER, .integer = 0 };
    literal_bang (&a);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Decimal (non-zero -> false)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 42.0 };
    literal_bang (&a);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Decimal (zero -> true)")
  {
    a = (struct literal){ .type = LT_DECIMAL, .decimal = 0.0 };
    literal_bang (&a);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Complex (non-zero -> false)")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 3.0 + 4.0 * I };
    literal_bang (&a);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Complex (zero -> true)")
  {
    a = (struct literal){ .type = LT_COMPLEX, .cplx = 0.0 + 0.0 * I };
    literal_bang (&a);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("True -> false")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = true };
    literal_bang (&a);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("False -> true")
  {
    a = (struct literal){ .type = LT_BOOL, .bl = false };
    literal_bang (&a);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("String (non-empty -> false)")
  {
    a = (struct literal){ .type = LT_STRING, .str = strfcstr ("hello") };
    literal_bang (&a);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("String (empty -> true)")
  {
    a = (struct literal){ .type = LT_STRING, .str = { .data = "", .len = 0 } };
    literal_bang (&a);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Array (non-empty -> false)")
  {
    a = (struct literal){ .type = LT_ARRAY, .arr = { .len = 1 } };
    literal_bang (&a);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Array (empty -> true)")
  {
    a = (struct literal){ .type = LT_ARRAY, .arr = { .len = 0 } };
    literal_bang (&a);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }

  TEST_CASE ("Object (non-empty -> false)")
  {
    a = (struct literal){ .type = LT_OBJECT, .obj = { .len = 2 } };
    literal_bang (&a);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == false);
  }

  TEST_CASE ("Object (empty -> true)")
  {
    a = (struct literal){ .type = LT_OBJECT, .obj = { .len = 0 } };
    literal_bang (&a);
    test_assert (a.type == LT_BOOL);
    test_assert (a.bl == true);
  }
}
#endif
