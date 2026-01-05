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
 *   Implements semantic.h. Provides semantic validation of literal values
 *   against expected types, including range checking for primitives, structure
 *   field matching, union variant validation, and array length verification.
 */

#include <numstore/compiler/literal.h>
#include <numstore/compiler/semantic.h>
#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/core/math.h>
#include <numstore/core/random.h>
#include <numstore/core/strings_utils.h>
#include <numstore/test/testing.h>
#include <numstore/types/prim.h>
#include <numstore/types/types.h>

static inline bool
string_fits_fixed (const struct literal *lit, const struct type *expected)
{
  ASSERT (lit->type == LT_STRING);

  if (expected->type != T_SARRAY || expected->sa.rank != 1)
    {
      return false;
    }

  const struct type *elem = expected->sa.t;
  if (elem->type != T_PRIM)
    {
      return false;
    }

  bool byte_like = (elem->p == U8) || (elem->p == I8);
  return byte_like && (lit->str.len <= expected->sa.dims[0]);
}

#ifndef NTEST
TEST (TT_UNIT, string_fits_fixed)
{
  /* length 3 */
  test_assert (
      string_fits_fixed (
          &(struct literal){
              .type = LT_STRING,
              .str.len = 3,
          },
          &(struct type){
              .type = T_SARRAY,
              .sa = {
                  .rank = 1,
                  .dims = (u32[]){
                      4,
                  },
                  .t = &(struct type){
                      .type = T_PRIM,
                      .p = U8,
                  },
              },
          }));

  /* length 4 (exact fit) */
  test_assert (
      string_fits_fixed (
          &(struct literal){
              .type = LT_STRING,
              .str.len = 4,
          },
          &(struct type){
              .type = T_SARRAY,
              .sa = {
                  .rank = 1,
                  .dims = (u32[]){
                      4,
                  },
                  .t = &(struct type){
                      .type = T_PRIM,
                      .p = U8,
                  },
              },
          }));

  /* length 0 (empty string) */
  test_assert (
      string_fits_fixed (
          &(struct literal){
              .type = LT_STRING,
              .str.len = 0,
          },
          &(struct type){
              .type = T_SARRAY,
              .sa = {
                  .rank = 1,
                  .dims = (u32[]){
                      4,
                  },
                  .t = &(struct type){
                      .type = T_PRIM,
                      .p = U8,
                  },
              },
          }));

  /* length 5 (overflow) */
  test_assert (
      !string_fits_fixed (
          &(struct literal){
              .type = LT_STRING,
              .str.len = 5,
          },
          &(struct type){
              .type = T_SARRAY,
              .sa = {
                  .rank = 1,
                  .dims = (u32[]){
                      4,
                  },
                  .t = &(struct type){
                      .type = T_PRIM,
                      .p = U8,
                  },
              },
          }));

  /* Different prim element (F32) */
  test_assert (
      !string_fits_fixed (
          &(struct literal){
              .type = LT_STRING,
              .str.len = 1,
          },
          &(struct type){
              .type = T_SARRAY,
              .sa = {
                  .rank = 1,
                  .dims = (u32[]){
                      4,
                  },
                  .t = &(struct type){
                      .type = T_PRIM,
                      .p = F32,
                  },
              },
          }));

  /* Rank‑2 static arr */
  test_assert (
      !string_fits_fixed (
          &(struct literal){
              .type = LT_STRING,
              .str.len = 1,
          },
          &(struct type){
              .type = T_SARRAY,
              .sa = {
                  .rank = 2,
                  .dims = (u32[]){
                      1,
                      4,
                  },
                  .t = &(struct type){
                      .type = T_PRIM,
                      .p = U8,
                  },
              },
          }));

  /* Expected type not an arr (CU16 prim) */
  test_assert (
      !string_fits_fixed (
          &(struct literal){
              .type = LT_STRING,
              .str.len = 1,
          },
          &(struct type){
              .type = T_PRIM,
              .p = CU16,
          }));
}
#endif

static bool
integer_in_range (i32 integer, enum prim_t ttype)
{
  ASSERT (prim_is_int (ttype) || prim_is_complex (ttype) || prim_is_float (ttype));

  switch (ttype)
    {
    case I8:
    case CI16:
      {
        return integer >= I8_MIN && integer <= I8_MAX;
      }

    case U8:
    case CU16:
      {
        return integer >= 0 && integer <= U8_MAX;
      }

    case I16:
    case CI32:
      {
        return integer >= I16_MIN && integer <= I16_MAX;
      }

    case U16:
    case CU32:
      {
        return integer >= 0 && integer <= U16_MAX;
      }

    case I32:
    case CI64:
      {
        return true;
      }

    case U32:
    case CU64:
      {
        return integer >= 0;
      }

    case I64:
    case CI128:
      {
        return true;
      }
    case U64:
    case CU128:
      {
        return integer >= 0;
      }

    case F16:
    case CF32:
      {
        return integer >= (i32)F16_MIN && integer <= (i32)F16_MAX;
      }

    case F32:
    case CF64:
    case F64:
    case CF128:
    case F128:
    case CF256:
      {
        return true;
      }

    default:
      {
        UNREACHABLE ();
      }
    }
  UNREACHABLE ();
}

#ifndef NTEST
typedef struct
{
  i32 lv;         // literal value
  enum prim_t ct; // coerce type
  bool exp;       // Can coerce lv into ct
} iirtc;          // integer in range test case

static void
test_integer_in_range_case (iirtc *cases, u32 len)
{
  for (u32 i = 0; i < len; ++i)
    {
      if (cases[i].exp)
        {
          test_assert_more (integer_in_range (cases[i].lv, cases[i].ct),
                            "lv = %d ct = %s",
                            cases[i].lv, prim_to_str (cases[i].ct));
        }
      else
        {
          test_assert_more (!integer_in_range (cases[i].lv, cases[i].ct),
                            "lv = %d ct = %s",
                            cases[i].lv, prim_to_str (cases[i].ct));
        }
    }
}

#ifndef NTEST
TEST (TT_UNIT, integer_in_range)
{
  iirtc cases[] = {
    // I8
    { .lv = I8_MIN, .ct = I8, .exp = true },
    { .lv = I8_MAX, .ct = I8, .exp = true },
    { .lv = (i64)I8_MIN - 1, .ct = I8, .exp = false },
    { .lv = (i64)I8_MAX + 1, .ct = I8, .exp = false },
    { .lv = randi8 (), .ct = I8, .exp = true },
    { .lv = randi32r (I8_MAX + 1, I8_MAX + 5), .ct = I8, .exp = false },
    { .lv = randi32r (I8_MIN - 5, I8_MIN - 1), .ct = I8, .exp = false },
    { .lv = 0, .ct = I8, .exp = true },

    // U8
    { .lv = 0, .ct = U8, .exp = true },
    { .lv = U8_MAX, .ct = U8, .exp = true },
    { .lv = -1, .ct = U8, .exp = false },
    { .lv = U8_MAX + 1, .ct = U8, .exp = false },
    { .lv = (i32)randu8 (), .ct = U8, .exp = true },
    { .lv = randu32r (U8_MAX + 1, U8_MAX + 10), .ct = U8, .exp = false },

    // I16
    { .lv = I16_MIN, .ct = I16, .exp = true },
    { .lv = I16_MAX, .ct = I16, .exp = true },
    { .lv = I16_MIN - 1, .ct = I16, .exp = false },
    { .lv = I16_MAX + 1, .ct = I16, .exp = false },
    { .lv = randi16 (), .ct = I16, .exp = true },
    { .lv = randi32r (I16_MAX + 100, I16_MAX + 200), .ct = I16, .exp = false },
    { .lv = randi32r (I16_MIN - 200, I16_MIN - 100), .ct = I16, .exp = false },
    { .lv = 0, .ct = I16, .exp = true },

    // U16
    { .lv = 0, .ct = U16, .exp = true },
    { .lv = U16_MAX, .ct = U16, .exp = true },
    { .lv = -1, .ct = U16, .exp = false },
    { .lv = U16_MAX + 1, .ct = U16, .exp = false },
    { .lv = randu16 (), .ct = U16, .exp = true },
    { .lv = randi32r (U16_MAX + 1000, U16_MAX + 2000), .ct = U16, .exp = false },

    // I32
    { .lv = I32_MIN, .ct = I32, .exp = true },
    { .lv = I32_MAX, .ct = I32, .exp = true },
    { .lv = randi32 (), .ct = I32, .exp = true },
    { .lv = 0, .ct = I32, .exp = true },

    // U32
    { .lv = 0, .ct = U32, .exp = true },
    { .lv = -1, .ct = U32, .exp = false },
    { .lv = randi32r (0, I32_MAX), .ct = U32, .exp = true },

    // I64
    { .lv = randi32 (), .ct = I64, .exp = true },
    { .lv = 0, .ct = I64, .exp = true },

    // U64
    { .lv = 0, .ct = U64, .exp = true },
    { .lv = -1, .ct = U64, .exp = false },

    // F16
    { .lv = (i32)F16_MIN, .ct = F16, .exp = true },
    { .lv = (i32)F16_MAX, .ct = F16, .exp = true },
    { .lv = (i32)F16_MIN - 1, .ct = F16, .exp = false },
    { .lv = (i32)F16_MAX + 1, .ct = F16, .exp = false },
    { .lv = randi32r ((i32)F16_MIN, (i32)F16_MAX), .ct = F16, .exp = true },
    { .lv = randi32r ((i32)F16_MAX + 100, (i32)F16_MAX + 200), .ct = F16, .exp = false },
    { .lv = randi32r ((i32)F16_MIN - 200, (i32)F16_MIN - 100), .ct = F16, .exp = false },
    { .lv = 0, .ct = F16, .exp = true },

    // F32
    { .lv = (i32)F32_MIN, .ct = F32, .exp = true },
    { .lv = (i32)F32_MAX, .ct = F32, .exp = true },
    { .lv = randi32r ((i32)F32_MIN, (i32)F32_MAX), .ct = F32, .exp = true },
    { .lv = 0, .ct = F32, .exp = true },

    // F64
    { .lv = randi32 (), .ct = F64, .exp = true },

    // F128
    { .lv = randi32 (), .ct = F128, .exp = true },
  };

  test_integer_in_range_case (cases, arrlen (cases));
}
#endif
#endif

static bool
decimal_in_range (f32 decimal, enum prim_t ttype)
{
  ASSERT (prim_is_int (ttype) || prim_is_complex (ttype) || prim_is_float (ttype));

  switch (ttype)
    {
    case I8:
    case CI16:
      {
        return decimal >= (f32)I8_MIN && decimal <= (f32)I8_MAX;
      }

    case U8:
    case CU16:
      {
        return decimal >= 0.0f && decimal <= (f32)U8_MAX;
      }

    case I16:
    case CI32:
      {
        return decimal >= (f32)I16_MIN && decimal <= (f32)I16_MAX;
      }

    case U16:
    case CU32:
      {
        return decimal >= 0.0f && decimal <= (f32)U16_MAX;
      }

    case I32:
    case CI64:
      {
        return decimal >= (f32)I32_MIN && decimal <= (f32)I32_MAX;
      }

    case U32:
    case CU64:
      {
        return decimal >= 0.0f && decimal <= (f32)U32_MAX;
      }

    case I64:
    case CI128:
      {
        return decimal >= (f32)I64_MIN && decimal <= (f32)I64_MAX;
      }

    case U64:
    case CU128:
      {
        return decimal >= 0.0f && decimal <= (f32)U64_MAX;
      }

    case F16:
    case CF32:
      {
        return decimal >= F16_MIN && decimal <= F16_MAX;
      }

    case F32:
    case CF64:
    case F64:
    case CF128:
    case F128:
    case CF256:
      {
        return true;
      }

    default:
      {
        UNREACHABLE ();
      }
    }
}

#ifndef NTEST

typedef struct
{
  float val;      // decimal value
  enum prim_t ct; // target type
  bool exp;       // expected result
} ddrtc;

static void
test_decimal_in_range_case (const ddrtc *cases, u32 len)
{
  for (u32 i = 0; i < len; ++i)
    {
      if (cases[i].exp)
        {
          test_assert_more (
              decimal_in_range (cases[i].val, cases[i].ct),
              "val = %f ct = %s expected %d",
              cases[i].val, prim_to_str (cases[i].ct), cases[i].exp);
        }
      else
        {
          test_assert_more (
              !decimal_in_range (cases[i].val, cases[i].ct),
              "val = %f ct = %s expected %d",
              cases[i].val, prim_to_str (cases[i].ct), cases[i].exp);
        }
    }
}

#ifndef NTEST
TEST (TT_UNIT, decimal_in_range)
{
  const ddrtc cases[] = {
    // I8
    { .val = (f32)I8_MIN, .ct = I8, .exp = true },
    { .val = (f32)I8_MAX, .ct = I8, .exp = true },
    { .val = (f32)I8_MIN - 1, .ct = I8, .exp = false },
    { .val = (f32)I8_MAX + 1, .ct = I8, .exp = false },
    { .val = 0.0f, .ct = I8, .exp = true },

    // U8
    { .val = 0.0f, .ct = U8, .exp = true },
    { .val = (f32)U8_MAX, .ct = U8, .exp = true },
    { .val = -1.0f, .ct = U8, .exp = false },
    { .val = (f32)U8_MAX + 1, .ct = U8, .exp = false },
    { .val = 42.0f, .ct = U8, .exp = true },

    // I16
    { .val = (f32)I16_MIN, .ct = I16, .exp = true },
    { .val = (f32)I16_MAX, .ct = I16, .exp = true },
    { .val = (f32)I16_MIN - 1, .ct = I16, .exp = false },
    { .val = (f32)I16_MAX + 1, .ct = I16, .exp = false },
    { .val = 1234.0f, .ct = I16, .exp = true },

    // U16
    { .val = 0.0f, .ct = U16, .exp = true },
    { .val = (f32)U16_MAX, .ct = U16, .exp = true },
    { .val = -1.0f, .ct = U16, .exp = false },
    { .val = (f32)U16_MAX + 1, .ct = U16, .exp = false },
    { .val = 500.0f, .ct = U16, .exp = true },

    // I32
    { .val = (f32)I32_MIN, .ct = I32, .exp = true },
    { .val = (f32)I32_MAX, .ct = I32, .exp = true },
    { .val = 0.0f, .ct = I32, .exp = true },
    { .val = 123456789.0f, .ct = I32, .exp = true },

    // U32
    { .val = 0.0f, .ct = U32, .exp = true },
    { .val = (f32)I32_MAX, .ct = U32, .exp = true },
    { .val = -1.0f, .ct = U32, .exp = false },
    { .val = (f32)I32_MIN, .ct = U32, .exp = false },

    // I64
    { .val = -F32_MAX, .ct = I64, .exp = false },
    { .val = F32_MAX, .ct = I64, .exp = false },
    { .val = F32_MAX + 1000.0f, .ct = I64, .exp = false },

    // U64
    { .val = 0.0f, .ct = U64, .exp = true },
    { .val = F32_MAX, .ct = U64, .exp = false },
    { .val = -1.0f, .ct = U64, .exp = false },
    { .val = F32_MAX + 1.0f, .ct = U64, .exp = false },

    // F16
    { .val = F16_MIN, .ct = F16, .exp = true },
    { .val = F16_MAX, .ct = F16, .exp = true },
    { .val = F16_MIN - 1.0f, .ct = F16, .exp = false },
    { .val = F16_MAX + 1.0f, .ct = F16, .exp = false },

    // F32
    { .val = F32_MIN, .ct = F32, .exp = true },
    { .val = F32_MAX, .ct = F32, .exp = true },

    // F64
    { .val = -F32_MAX, .ct = F64, .exp = true },
    { .val = F32_MAX, .ct = F64, .exp = true },

    // F128
    { .val = -F32_MAX, .ct = F128, .exp = true },
    { .val = F32_MAX, .ct = F128, .exp = true },
  };

  test_decimal_in_range_case (cases, sizeof cases / sizeof *cases);
}
#endif

#endif

static bool
cmplx_in_range (cf64 cmplx, enum prim_t ttype)
{
  ASSERT (prim_is_complex (ttype));

  f32 real = crealf (cmplx);
  f32 imag = cimagf (cmplx);

  switch (ttype)
    {
    case CI16:
      {
        bool real_ok = real >= (f32)I8_MIN && real <= (f32)I8_MAX;
        bool imag_ok = imag >= (f32)I8_MIN && imag <= (f32)I8_MAX;
        return real_ok && imag_ok;
      }
    case CU16:
      {
        bool real_ok = real >= 0.0f && real <= (f32)U8_MAX;
        bool imag_ok = imag >= 0.0f && imag <= (f32)U8_MAX;
        return real_ok && imag_ok;
      }
    case CI32:
      {
        bool real_ok = real >= (f32)I16_MIN && real <= (f32)I16_MAX;
        bool imag_ok = imag >= (f32)I16_MIN && imag <= (f32)I16_MAX;
        return real_ok && imag_ok;
      }
    case CU32:
      {
        bool real_ok = real >= 0.0f && real <= (f32)U16_MAX;
        bool imag_ok = imag >= 0.0f && imag <= (f32)U16_MAX;
        return real_ok && imag_ok;
      }
    case CI64:
      {
        bool real_ok = real >= (f32)I32_MIN && real <= (f32)I32_MAX;
        bool imag_ok = imag >= (f32)I32_MIN && imag <= (f32)I32_MAX;
        return real_ok && imag_ok;
      }
    case CU64:
      {
        bool real_ok = real >= 0.0f && real <= (f32)U32_MAX;
        bool imag_ok = imag >= 0.0f && imag <= (f32)U32_MAX;
        return real_ok && imag_ok;
      }
    case CI128:
      {
        bool real_ok = real >= -F32_MAX && real <= F32_MAX;
        bool imag_ok = imag >= -F32_MAX && imag <= F32_MAX;
        return real_ok && imag_ok;
      }
    case CU128:
      {
        bool real_ok = real >= 0.0f && real <= F32_MAX;
        bool imag_ok = imag >= 0.0f && imag <= F32_MAX;
        return real_ok && imag_ok;
      }
    case CF32:
      {
        bool real_ok = real >= F16_MIN && real <= F16_MAX;
        bool imag_ok = imag >= F16_MIN && imag <= F16_MAX;
        return real_ok && imag_ok;
      }
    case CF64:
    case CF128:
    case CF256:
      return true;
    default:
      UNREACHABLE ();
    }
}

#ifndef NTEST

typedef struct
{
  f32 real;
  f32 imag;
  enum prim_t ct;
  bool exp;
} cirtc;

static void
test_cmplx_in_range_case (const cirtc *cases, u32 len)
{
  for (u32 i = 0; i < len; ++i)
    {
      f32 real = cases[i].real;
      f32 imag = cases[i].imag;
      enum prim_t ct = cases[i].ct;
      bool exp = cases[i].exp;

      if (exp)
        {
          test_assert_more (
              cmplx_in_range (real + I * imag, ct),
              "real = %f imag = %f ct = %s",
              real, imag, prim_to_str (ct));
        }
      else
        {
          test_assert_more (
              !cmplx_in_range (real + I * imag, ct),
              "real = %f imag = %f ct = %s",
              real, imag, prim_to_str (ct));
        }
    }
}

#ifndef NTEST
TEST (TT_UNIT, cmplx_in_range)
{
  const cirtc cases[] = {
    // CI16
    { .real = 0.0f, .imag = 0.0f, .ct = CI16, .exp = true },
    { .real = (f32) (I8_MAX + 1), .imag = 0.0f, .ct = CI16, .exp = false },
    { .real = 0.0f, .imag = (f32) (I8_MIN - 1), .ct = CI16, .exp = false },

    // CU16
    { .real = 0.0f, .imag = 0.0f, .ct = CU16, .exp = true },
    { .real = -1.0f, .imag = 0.0f, .ct = CU16, .exp = false },
    { .real = 0.0f, .imag = -1.0f, .ct = CU16, .exp = false },
    { .real = (f32) (U8_MAX + 1), .imag = 0.0f, .ct = CU16, .exp = false },

    // CF64
    { .real = F32_MIN, .imag = F32_MAX, .ct = CF64, .exp = true },
    { .real = (F32_MIN - 1.0f), .imag = 0.0f, .ct = CF64, .exp = true },

    // CF128
    { .real = 999999.0f, .imag = -999999.0f, .ct = CF128, .exp = true },
  };

  test_cmplx_in_range_case (cases, sizeof cases / sizeof *cases);
}
#endif

#endif

static err_t
validate_prim (const struct literal *lit, const struct type *expected, error *e)
{
  if (expected->type != T_PRIM)
    {
      goto failed;
    }

  enum prim_t p = expected->p;

  switch (lit->type)
    {
    case LT_INTEGER:
      {
        if (!integer_in_range (lit->integer, p))
          {
            return error_causef (
                e, ERR_ARITH,
                "Literal: %d is out of bounds for primitive of "
                "type: %s",
                lit->integer, prim_to_str (p));
          }
        return SUCCESS;
      }

    case LT_DECIMAL:
      {
        if (!decimal_in_range (lit->decimal, p))
          {
            return error_causef (
                e, ERR_ARITH,
                "Literal: %f is out of bounds for primitive of "
                "type: %s",
                lit->decimal, prim_to_str (p));
          }
        return SUCCESS;
      }

    case LT_COMPLEX:
      {
        if (!prim_is_complex (p))
          {
            return error_causef (
                e, ERR_SYNTAX,
                "Cannot cast complex type: %f + %f * I to lower scalar of type: %s",
                crealf (lit->cplx), cimagf (lit->cplx), prim_to_str (p));
          }
        if (!cmplx_in_range (lit->cplx, p))
          {
            return error_causef (
                e, ERR_ARITH,
                "Literal: %f + %f * I is out of bounds for primitive of "
                "type: %s",
                crealf (lit->cplx), cimagf (lit->cplx), prim_to_str (p));
          }
        return SUCCESS;
      }

    case LT_BOOL:
      {
        if (p == U8 || p == I8)
          {
            return SUCCESS;
          }
        break;
      }

    default:
      {
        UNREACHABLE ();
      }
    }

failed:
  return error_causef (e, ERR_SYNTAX, "Invalid prim literal/type match");
}

#ifndef NTEST
static void
test_validate_integer_case (i32 integer, enum prim_t prim, err_t expected)
{
  error e = error_create ();
  const struct literal l = { .type = LT_INTEGER, .integer = integer };
  const struct type t = { .type = T_PRIM, .p = prim };

  i_log_info ("%s %s %d\n", prim_to_str (prim), err_t_to_str (expected), integer);
  test_assert_int_equal (expected, validate_prim (&l, &t, &e));
}
static void
test_validate_decimal_case (f32 decimal, enum prim_t prim, err_t expected)
{
  error e = error_create ();
  const struct literal l = { .type = LT_DECIMAL, .decimal = decimal };
  const struct type t = { .type = T_PRIM, .p = prim };

  test_assert_int_equal (expected, validate_prim (&l, &t, &e));
}
static void
test_validate_cmplx_case (cf64 cmplx, enum prim_t prim, err_t expected)
{
  error e = error_create ();
  const struct literal l = { .type = LT_COMPLEX, .cplx = cmplx };
  const struct type t = { .type = T_PRIM, .p = prim };

  test_assert_int_equal (expected, validate_prim (&l, &t, &e));
}

#ifndef NTEST
TEST (TT_UNIT, validate_prim_integer)
{
  struct
  {
    i32 integer;
    enum prim_t prim;
    err_t expected;
  } cases[] = {
    /* Integer primitives */
    { 0, I8, SUCCESS },
    { I8_MAX, I8, SUCCESS },
    { I8_MIN, I8, SUCCESS },
    { I8_MAX + 1, I8, ERR_ARITH },
    { I8_MIN - 1, I8, ERR_ARITH },

    { 0, U8, SUCCESS },
    { U8_MAX, U8, SUCCESS },
    { -1, U8, ERR_ARITH },
    { U8_MAX + 1, U8, ERR_ARITH },

    { I16_MIN, I16, SUCCESS },
    { I16_MAX, I16, SUCCESS },
    { I16_MIN - 1, I16, ERR_ARITH },

    { U16_MAX, U16, SUCCESS },
    { -1, U16, ERR_ARITH },

    { I32_MIN, I32, SUCCESS },
    { I32_MAX, I32, SUCCESS },

    { 0, U32, SUCCESS },
    { I32_MAX, U32, SUCCESS },
    { -1, U32, ERR_ARITH },

    { I32_MIN, I64, SUCCESS },
    { I32_MAX, I64, SUCCESS },

    { I32_MAX, U64, SUCCESS },
    { -1, U64, ERR_ARITH },

    /* Float targets (i32 fits in all of them) */
    { 0, F16, SUCCESS },
    { 2048, F16, SUCCESS },
    { F16_MAX, F16, SUCCESS },
    { -F16_MIN, F16, SUCCESS },
    { F16_MAX + 1, F16, ERR_ARITH },
    { F16_MIN - 1, F16, ERR_ARITH },

    { I32_MAX, F128, SUCCESS },
    { I32_MIN, F128, SUCCESS },

    /* Complex integer targets (imag=0) */
    { I8_MAX, CI16, SUCCESS },
    { I8_MIN, CI16, SUCCESS },
    { I8_MAX + 1, CI16, ERR_ARITH },

    { U8_MAX, CU16, SUCCESS },
    { -1, CU16, ERR_ARITH },

    { I16_MAX, CI32, SUCCESS },
    { I16_MIN - 1, CI32, ERR_ARITH },

    { U16_MAX, CU32, SUCCESS },
    { -1, CU32, ERR_ARITH },

    { I32_MIN, CI64, SUCCESS },
    { I32_MAX, CI64, SUCCESS },

    { I32_MAX, CU64, SUCCESS },
    { -1, CU64, ERR_ARITH },

    { 2048, CF32, SUCCESS },
    { -2049, CF32, SUCCESS },

    { I32_MIN, CF128, SUCCESS },
    { I32_MAX, CF128, SUCCESS },

    { I32_MIN, CF256, SUCCESS },
    { I32_MAX, CF256, SUCCESS },
  };

  for (u32 i = 0; i < arrlen (cases); ++i)
    {
      test_validate_integer_case (
          cases[i].integer,
          cases[i].prim,
          cases[i].expected);
    }
}
#endif

#ifndef NTEST
TEST (TT_UNIT, validate_prim_decimal)
{
  struct
  {
    f32 decimal;
    enum prim_t prim;
    err_t expected;
  } cases[] = {
    { 0.0f, I8, SUCCESS },
    { (f32)I8_MAX, I8, SUCCESS },
    { (f32)I8_MAX + 1, I8, ERR_ARITH },
    { (f32)I8_MIN, I8, SUCCESS },
    { (f32)I8_MIN - 1, I8, ERR_ARITH },

    { (f32)U8_MAX, U8, SUCCESS },
    { -1.0f, U8, ERR_ARITH },
    { (f32)U8_MAX + 1, U8, ERR_ARITH },

    { (f32)I16_MIN, I16, SUCCESS },
    { (f32)I16_MAX, I16, SUCCESS },
    { (f32)I16_MIN - 1, I16, ERR_ARITH },

    { (f32)U16_MAX, U16, SUCCESS },
    { -1.0f, U16, ERR_ARITH },

    { (f32)I32_MIN, I32, SUCCESS },
    { (f32)I32_MAX, I32, SUCCESS },

    { (f32)U32_MAX, U32, SUCCESS },
    { (f32)I32_MAX, U32, SUCCESS },
    { -1.0f, U32, ERR_ARITH },

    { (f32)I32_MAX, I64, SUCCESS },
    { (f32)I32_MIN, I64, SUCCESS },

    { (f32)I32_MAX, U64, SUCCESS },
    { -1.0f, U64, ERR_ARITH },

    /* Float types */
    { 0.0f, F16, SUCCESS },
    { F16_MAX, F16, SUCCESS },
    { F16_MAX + 1, F16, ERR_ARITH },
    { F16_MIN - 1, F16, ERR_ARITH },

    { 123.456f, F32, SUCCESS },
    { F32_MAX, F32, SUCCESS },

    { 1234567.0f, F64, SUCCESS },
    { F32_MIN, F64, SUCCESS },

    { 12345678.0f, F128, SUCCESS },

    /* Complex coercion (imag=0) */
    { (f32)I8_MAX, CI16, SUCCESS },
    { (f32)I8_MAX + 1, CI16, ERR_ARITH },

    { (f32)U8_MAX, CU16, SUCCESS },
    { -1.0f, CU16, ERR_ARITH },

    { (f32)I16_MAX, CI32, SUCCESS },
    { (f32)I16_MAX + 1, CI32, ERR_ARITH },

    { (f32)U16_MAX, CU32, SUCCESS },

    { (f32)I32_MIN, CI64, SUCCESS },
    { (f32)I32_MAX, CI64, SUCCESS },

    { (f32)I32_MAX, CU64, SUCCESS },
    { -1.0f, CU64, ERR_ARITH },

    { 2048.0f, CF32, SUCCESS },

    { F32_MAX, CF64, SUCCESS },

    { F32_MAX, CF128, SUCCESS },
    { F32_MIN, CF128, SUCCESS },

    { F32_MAX, CF256, SUCCESS },
  };

  for (u32 i = 0; i < arrlen (cases); ++i)
    {
      test_validate_decimal_case (
          cases[i].decimal,
          cases[i].prim,
          cases[i].expected);
    }
}
#endif

#ifndef NTEST
TEST (TT_UNIT, validate_prim_complex)
{
  struct
  {
    cf64 cmplx;
    enum prim_t prim;
    err_t expected;
  } cases[] = {
    /* Integer targets — imag must be 0 and real must be integral/in-range */
    { 0.0 + I * 0.0, I8, ERR_SYNTAX },
    { I8_MAX + I * 0.0, I8, ERR_SYNTAX },
    { (f64)I8_MAX + 1 + I * 0.0, I8, ERR_SYNTAX },
    { 1.5 + I * 0.0, I8, ERR_SYNTAX },
    { 1.0 + I * 1.0, I8, ERR_SYNTAX },

    { (f64)U8_MAX + I * 0.0, U8, ERR_SYNTAX },
    { -1.0 + I * 0.0, U8, ERR_SYNTAX },
    { (f64)U8_MAX + 1 + I * 0.0, U8, ERR_SYNTAX },
    { 42.0 + I * 3.0, U8, ERR_SYNTAX },

    /* Float targets — imag must be 0, real finite/in-range */
    { F16_MAX + I * 0.0, F16, ERR_SYNTAX },
    { F16_MIN + I * 0.0, F16, ERR_SYNTAX },
    { F16_MAX + 1 + I * 0.0, F16, ERR_SYNTAX },
    { F16_MAX + I * 1.0, F16, ERR_SYNTAX },

    { F32_MAX + I * 0.0, F32, ERR_SYNTAX },
    { (f64)F32_MAX + 1000 + I * 0.0, F32, ERR_SYNTAX },
    { 123.0 + I * 1.0, F32, ERR_SYNTAX },

    /* Complex integer targets */
    { I8_MAX + I * I8_MAX, CI16, SUCCESS },
    { (f64)I8_MAX + 1 + I * 0.0, CI16, ERR_ARITH },
    { 0.0 + I * (f64) (I8_MAX + 1), CI16, ERR_ARITH },

    { (f64)U8_MAX + I * (f64)U8_MAX, CU16, SUCCESS },
    { -1.0 + I * 0.0, CU16, ERR_ARITH },
    { 0.0 + I * -1.0, CU16, ERR_ARITH },

    { (f64)I16_MIN + I * (f64)I16_MAX, CI32, SUCCESS },
    { (f64)I16_MIN - 1 + I * 0.0, CI32, ERR_ARITH },

    /* Complex float targets */
    { F16_MIN + I * F16_MAX, CF32, SUCCESS },
    { (f64)F16_MAX + 1 + I * 0.0, CF32, ERR_ARITH },
    { F16_MAX + I * (F16_MAX + 1), CF32, ERR_ARITH },

    { F32_MIN + I * F32_MAX, CF64, SUCCESS },

    { 999999.0 + I * -999999.0, CF128, SUCCESS },

    { F32_MAX + I * F32_MAX, CF256, SUCCESS },
  };

  for (u32 i = 0; i < arrlen (cases); ++i)
    {
      test_validate_cmplx_case (
          cases[i].cmplx,
          cases[i].prim,
          cases[i].expected);
    }
}
#endif

#endif

static err_t
validate_object (const struct literal *lit, const struct type *expected, error *e)
{
  ASSERT (lit->type == LT_OBJECT);

  switch (expected->type)
    {
    case T_STRUCT:
      {
        if (lit->obj.len != expected->st.len)
          {
            goto failed;
          }

        for (u32 i = 0; i < expected->st.len; i++)
          {
            const struct string ekey = expected->st.keys[i];
            bool matched = false;

            for (u32 j = 0; j < lit->obj.len; j++)
              {
                if (!string_equal (ekey, lit->obj.keys[j]))
                  {
                    continue;
                  }

                matched = true;

                const struct literal *nextl = &lit->obj.literals[j];
                const struct type *nextt = &expected->st.types[i];

                err_t_wrap (literal_validate_type (NULL, nextl, nextt, e), e);
              }

            if (!matched)
              {
                goto failed;
              }
          }
        return SUCCESS;
      }

    case T_UNION:
      {
        if (lit->obj.len != 1)
          {
            goto failed;
          }

        for (u32 i = 0; i < expected->un.len; i++)
          {
            if (string_equal (expected->un.keys[i], lit->obj.keys[0]))
              {
                const struct literal *nextl = &lit->obj.literals[0];
                const struct type *nextt = &expected->st.types[i];

                /* TODO - get rid of recursion. Recursive validate inner type */
                err_t_wrap (literal_validate_type (NULL, nextl, nextt, e), e);
                return SUCCESS;
              }
          }

        goto failed;
      }

    default:
      {
        goto failed;
      }
    }

failed:
  return error_causef (e, ERR_SYNTAX, "Invalid object literal/type match");
}

#ifndef NTEST
TEST (TT_UNIT, validate_object)
{
  error err = error_create ();

  /* Struct success */
  test_assert_int_equal (
      SUCCESS,
      validate_object (
          /* { x: 5, y: 10 } */
          &(struct literal){
              .type = LT_OBJECT,
              .obj = {
                  .len = 2,
                  .keys = (struct string[]){
                      strfcstr ("x"),
                      strfcstr ("y"),
                  },
                  .literals = (struct literal[]){
                      {
                          .type = LT_INTEGER,
                          .integer = 5,
                      },
                      {
                          .type = LT_INTEGER,
                          .integer = 10,
                      },
                  },
              },
          },

          /* struct { x i32, y i32 } */
          &(struct type){
              .type = T_STRUCT,
              .st = {
                  .len = 2,
                  .keys = (struct string[]){
                      strfcstr ("x"),
                      strfcstr ("y"),
                  },
                  .types = (struct type[]){
                      {
                          .type = T_PRIM,
                          .p = I32,
                      },
                      {
                          .type = T_PRIM,
                          .p = I32,
                      },
                  },
              },
          },
          &err));

  /* Union success */
  test_assert_int_equal (
      SUCCESS,
      validate_object (

          /* { b: "s" } - note "s" matches [3]u8 */
          &(struct literal){
              .type = LT_OBJECT,
              .obj = {
                  .len = 1,
                  .keys = (struct string[]){
                      strfcstr ("b"),
                  },
                  .literals = (struct literal[]){
                      {
                          .type = LT_STRING,
                          .str = strfcstr ("s"),
                      },
                  },
              },
          },

          /* union { a i32, b [3]u8 } */
          &(struct type){
              .type = T_UNION,
              .un = {
                  .len = 2,
                  .keys = (struct string[]){
                      strfcstr ("a"),
                      strfcstr ("b"),
                  },
                  .types = (struct type[]){
                      {
                          .type = T_PRIM,
                          .p = I32,
                      },
                      {
                          .type = T_SARRAY,
                          .sa = (struct sarray_t){
                              .rank = 1,
                              .dims = (u32[]){ 3 },
                              .t = &(struct type){
                                  .type = T_PRIM,
                                  .p = U8,
                              },
                          },
                      },
                  },
              },
          },
          &err));

  /* nested struct */
  test_assert_int_equal (
      SUCCESS,
      validate_object (
          /* { point: { x: 1, y: 2 } } */
          &(struct literal){
              .type = LT_OBJECT,
              .obj = {
                  .len = 1,
                  .keys = (struct string[]){
                      strfcstr ("point"),
                  },
                  .literals = (struct literal[]){
                      {
                          .type = LT_OBJECT,
                          .obj = {
                              .len = 2,
                              .keys = (struct string[]){
                                  strfcstr ("x"),
                                  strfcstr ("y"),
                              },
                              .literals = (struct literal[]){
                                  {
                                      .type = LT_INTEGER,
                                      .integer = 1,
                                  },
                                  {
                                      .type = LT_INTEGER,
                                      .integer = 2,
                                  },
                              },
                          },
                      },
                  },
              },
          },

          /* struct { point struct { x i32, y i32 } } */
          &(struct type){
              .type = T_STRUCT,
              .st = {
                  .len = 1,
                  .keys = (struct string[]){
                      strfcstr ("point"),
                  },
                  .types = (struct type[]){
                      {
                          .type = T_STRUCT,
                          .st = {
                              .len = 2,
                              .keys = (struct string[]){
                                  strfcstr ("x"),
                                  strfcstr ("y"),
                              },
                              .types = (struct type[]){
                                  {
                                      .type = T_PRIM,
                                      .p = I32,
                                  },
                                  {
                                      .type = T_PRIM,
                                      .p = I32,
                                  },
                              },
                          },
                      },
                  },
              },
          },
          &err));

  /* Union with array variant */
  test_assert_int_equal (
      SUCCESS,
      validate_object (
          /* { coords: [1, 2, 3] } */
          &(struct literal){
              .type = LT_OBJECT,
              .obj = {
                  .len = 1,
                  .keys = (struct string[]){
                      strfcstr ("coords"),
                  },
                  .literals = (struct literal[]){
                      {
                          .type = LT_ARRAY,
                          .arr = (struct array){
                              .len = 3,
                              .literals = (struct literal[]){
                                  { .type = LT_INTEGER, .integer = 1 },
                                  { .type = LT_INTEGER, .integer = 2 },
                                  { .type = LT_INTEGER, .integer = 3 },
                              },
                          },
                      },
                  },
              },
          },

          /* union { id: u8, coords: [3]i32 } */
          &(struct type){
              .type = T_UNION,
              .un = {
                  .len = 2,
                  .keys = (struct string[]){
                      strfcstr ("id"),
                      strfcstr ("coords"),
                  },
                  .types = (struct type[]){
                      {
                          .type = T_PRIM,
                          .p = U8,
                      },
                      {
                          .type = T_SARRAY,
                          .sa = {
                              .rank = 1,
                              .dims = (u32[]){ 3 },
                              .t = &(struct type){
                                  .type = T_PRIM,
                                  .p = I32,
                              },
                          },
                      },
                  },
              },
          },
          &err));

  /* Union with casted literal type */
  test_assert_int_equal (
      SUCCESS,
      validate_object (
          /* { status: 5 } */
          &(struct literal){
              .type = LT_OBJECT,
              .obj = {
                  .len = 1,
                  .keys = (struct string[]){
                      strfcstr ("status"),
                  },
                  .literals = (struct literal[]){
                      {
                          .type = LT_DECIMAL,
                          .decimal = 1,
                      },
                  },
              },
          },

          /* union { status: i32, message: u8 } */
          &(struct type){
              .type = T_UNION,
              .un = {
                  .len = 2,
                  .keys = (struct string[]){
                      strfcstr ("status"),
                      strfcstr ("message"),
                  },
                  .types = (struct type[]){
                      {
                          .type = T_PRIM,
                          .p = I32,
                      },
                      {
                          .type = T_PRIM,
                          .p = U8,
                      },
                  },
              },
          },
          &err));

  /* Union with out of bound cast */
  test_assert_int_equal (
      ERR_ARITH,
      validate_object (
          /* { status: OOB } */
          &(struct literal){
              .type = LT_OBJECT,
              .obj = {
                  .len = 1,
                  .keys = (struct string[]){
                      strfcstr ("status"),
                  },
                  .literals = (struct literal[]){
                      {
                          .type = LT_DECIMAL,
                          .decimal = F32_MAX,
                      },
                  },
              },
          },

          /* union { status: i32, message: u8 } */
          &(struct type){
              .type = T_UNION,
              .un = {
                  .len = 2,
                  .keys = (struct string[]){
                      strfcstr ("status"),
                      strfcstr ("message"),
                  },
                  .types = (struct type[]){
                      {
                          .type = T_PRIM,
                          .p = I32,
                      },
                      {
                          .type = T_PRIM,
                          .p = U8,
                      },
                  },
              },
          },
          &err));
  error_reset (&err);

  /* Union with mismatched literal type */
  test_assert_int_equal (
      ERR_SYNTAX,
      validate_object (
          /* { status: "foobar" } */
          &(struct literal){
              .type = LT_OBJECT,
              .obj = {
                  .len = 1,
                  .keys = (struct string[]){
                      strfcstr ("status"),
                  },
                  .literals = (struct literal[]){
                      {
                          .type = LT_STRING,
                          .str = strfcstr ("foobar"),
                      },
                  },
              },
          },

          /* union { status: i32, message: u8 } */
          &(struct type){
              .type = T_UNION,
              .un = {
                  .len = 2,
                  .keys = (struct string[]){
                      strfcstr ("status"),
                      strfcstr ("message"),
                  },
                  .types = (struct type[]){
                      {
                          .type = T_PRIM,
                          .p = I32,
                      },
                      {
                          .type = T_PRIM,
                          .p = U8,
                      },
                  },
              },
          },
          &err));
  error_reset (&err);

  /* Struct with wrong field name */
  test_assert_int_equal (
      ERR_SYNTAX,
      validate_object (
          /* { wrong: 42 } */
          &(struct literal){
              .type = LT_OBJECT,
              .obj = {
                  .len = 1,
                  .keys = (struct string[]){
                      strfcstr ("wrong"),
                  },
                  .literals = (struct literal[]){
                      {
                          .type = LT_INTEGER,
                          .integer = 42,
                      },
                  },
              },
          },

          /* struct { expected i32 } */
          &(struct type){
              .type = T_STRUCT,
              .st = {
                  .len = 1,
                  .keys = (struct string[]){
                      strfcstr ("expected"),
                  },
                  .types = (struct type[]){
                      {
                          .type = T_PRIM,
                          .p = I32,
                      },
                  },
              },
          },
          &err));
}
#endif

static err_t
validate_arr (
    bool *islist,
    const struct literal *lit,
    const struct type *expected,
    error *e)
{
  ASSERT (lit->type == LT_ARRAY);

  if (expected->type == T_SARRAY)
    {
      if (lit->arr.len != expected->sa.dims[0])
        {
          goto failed;
        }

      for (u32 i = 0; i < lit->arr.len; i++)
        {
          const struct literal *nextl = &lit->arr.literals[i];
          err_t_wrap (literal_validate_type (NULL, nextl, expected->sa.t, e), e);
        }

      return SUCCESS;
    }

  if (islist && !*islist)
    {
      *islist = true;

      for (u32 i = 0; i < lit->arr.len; i++)
        {
          const struct literal *nextl = &lit->arr.literals[i];
          err_t_wrap (literal_validate_type (NULL, nextl, expected, e), e);
        }

      return SUCCESS;
    }

failed:
  return error_causef (e, ERR_SYNTAX, "Invalid arr/list literal/type match");
}

#ifndef NTEST
TEST (TT_UNIT, validate_arr)
{
  error err = error_create ();

  /* Static array success */
  test_assert_int_equal (
      SUCCESS,
      validate_arr (
          &(bool){ false },
          /* [1, 2, 3] */
          &(struct literal){
              .type = LT_ARRAY,
              .arr = {
                  .len = 3,
                  .literals = (struct literal[]){
                      {
                          .type = LT_INTEGER,
                          .integer = 1,
                      },
                      {
                          .type = LT_INTEGER,
                          .integer = 2,
                      },
                      {
                          .type = LT_INTEGER,
                          .integer = 3,
                      },
                  },
              },
          },
          /* [3]i32 */
          &(struct type){
              .type = T_SARRAY,
              .sa = {
                  .rank = 1,
                  .dims = (u32[]){ 3 },
                  .t = &(struct type){
                      .type = T_PRIM,
                      .p = I32,
                  },
              },
          },
          &err));
}
#endif

err_t
literal_validate_type (bool *islist, const struct literal *lit, const struct type *expected, error *e)
{
  /* Strings are interpreted as [N]u8/i8 just need to validate len < N */
  if (lit->type == LT_STRING && string_fits_fixed (lit, expected))
    {
      return SUCCESS;
    }

  switch (lit->type)
    {
    case LT_OBJECT:
      {
        return validate_object (lit, expected, e);
      }

    case LT_ARRAY:
      {
        return validate_arr (islist, lit, expected, e);
      }

    case LT_INTEGER:
    case LT_DECIMAL:
    case LT_COMPLEX:
    case LT_BOOL:
      {
        return validate_prim (lit, expected, e);
      }

    default:
      break;
    }

  return error_causef (e, ERR_SYNTAX, "Unknown literal/type match failure");
}

#ifndef NTEST
TEST (TT_UNIT, literal_validate_type)
{
  error err = error_create ();

  /* "abc" against [4]u8 — passes */
  test_assert_int_equal (
      SUCCESS,
      literal_validate_type (
          &(bool){ false },
          /* "abc" */
          &(struct literal){
              .type = LT_STRING,
              .str = strfcstr ("abc"),
          },
          /* [4]u8 */
          &(struct type){
              .type = T_SARRAY,
              .sa = {
                  .rank = 1,
                  .dims = (u32[]){ 4 },
                  .t = &(struct type){
                      .type = T_PRIM,
                      .p = U8,
                  },
              },
          },
          &err));
}
#endif
