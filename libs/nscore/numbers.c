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
 *   Implements numbers.h. Number parsing and Python-style modulo operations.
 */

#include <numstore/core/numbers.h>

#include <numstore/core/assert.h>
#include <numstore/core/bounds.h>
#include <numstore/core/macros.h>
#include <numstore/test/testing.h>

// core
err_t
parse_i32_expect (i32 *dest, const struct string data, error *e)
{
  ASSERT (data.data);
  ASSERT (data.len > 0);
  ASSERT (dest);

  u32 i = 0;
  bool neg = false;

  if (data.data[i] == '+' || data.data[i] == '-')
    {
      neg = (data.data[i] == '-');
      i++;
      ASSERT (i < data.len); /* We expect string to be valid */
    }

  i32 acc = 0;

  for (; i < data.len; i++)
    {
      char c = data.data[i];
      ASSERT (is_num (c));

      i32 digit = c - '0';

      if (!SAFE_MUL_I32 (&acc, acc, 10))
        {
          goto failed;
        }

      if (!SAFE_SUB_I32 (&acc, acc, digit))
        {
          goto failed;
        }
    }

  if (!neg)
    {
      if (acc == I32_MIN)
        {
          goto failed;
        }
      acc = -acc;
    }

  *dest = acc;
  return SUCCESS;

failed:
  return error_causef (
      e, ERR_ARITH,
      "Parse I32: Arithmetic Exception");
}

#ifndef NTEST
TEST (TT_UNIT, parse_i32_expect)
{
  i32 out = -1;
  error e = error_create ();

  const struct string s1 = (struct string){
    .data = "1234",
    .len = i_strlen ("1234"),
  };
  test_assert_int_equal (parse_i32_expect (&out, s1, &e), SUCCESS);
  test_assert_int_equal (out, 1234);

  const struct string s2 = (struct string){
    .data = "-56",
    .len = i_strlen ("-56"),
  };
  test_assert_int_equal (parse_i32_expect (&out, s2, &e), SUCCESS);
  test_assert_int_equal (out, -56);

  char *big = "999999999999999999999999999999999999999999";
  const struct string s3 = (struct string){
    .data = big,
    .len = i_strlen (big),
  };
  test_assert_int_equal (parse_i32_expect (&out, s3, &e), ERR_ARITH);
}
#endif

err_t
parse_f32_expect (f32 *dest, const struct string src, error *e)
{
  ASSERT (src.data);
  ASSERT (dest);
  ASSERT (src.len > 0);

  u32 len = src.len;
  char *s = src.data;
  u32 i = 0;
  bool neg = false;

  if (s[i] == '+' || s[i] == '-')
    {
      neg = (s[i] == '-');
      i++;
      ASSERT (i < len);
    }

  /* Integer part */
  f32 acc = 0.0f;
  bool saw_digit = false;
  while (i < len && s[i] >= '0' && s[i] <= '9')
    {
      f32 d = (f32) (s[i] - '0');
      if (!SAFE_MUL_F32 (&acc, acc, 10.0f))
        {
          goto failed;
        }
      if (!SAFE_ADD_F32 (&acc, acc, d))
        {
          goto failed;
        }
      i++;
      saw_digit = true;
    }

  /* Fractional part */
  if (i < len && s[i] == '.')
    {
      i++;
      ASSERT (i < len); /* cannot end with '.' */
      f32 frac = 0.0f, scale = 1.0f;
      while (i < len && s[i] >= '0' && s[i] <= '9')
        {
          f32 d = (f32) (s[i] - '0');
          if (!SAFE_MUL_F32 (&frac, frac, 10.0f))
            {
              goto failed;
            }
          if (!SAFE_ADD_F32 (&frac, frac, d))
            {
              goto failed;
            }
          if (!SAFE_MUL_F32 (&scale, scale, 10.0f))
            {
              goto failed;
            }
          i++;
          saw_digit = true;
        }
      f32 tmp;
      if (!SAFE_DIV_F32 (&tmp, frac, scale))
        {
          goto failed;
        }
      if (!SAFE_ADD_F32 (&acc, acc, tmp))
        {
          goto failed;
        }
    }

  ASSERT (saw_digit);

  /* Exponent part */
  if (i < len && (s[i] == 'e' || s[i] == 'E'))
    {
      i++;
      ASSERT (i < len); /* must have exponent digits */
      bool exp_neg = false;
      if (s[i] == '+' || s[i] == '-')
        {
          exp_neg = (s[i] == '-');
          i++;
          ASSERT (i < len);
        }
      u32 exp = 0;
      bool saw_exp = false;
      while (i < len && s[i] >= '0' && s[i] <= '9')
        {
          u32 d = (u32) (s[i] - '0');
          ASSERT (exp <= (UINT32_MAX - d) / 10);
          exp = exp * 10 + d;
          i++;
          saw_exp = true;
        }
      ASSERT (saw_exp);

      /* Apply exponent */
      for (u32 k = 0; k < exp; k++)
        {
          if (exp_neg)
            {
              if (!SAFE_DIV_F32 (&acc, acc, 10.0f))
                {
                  goto failed;
                }
            }
          else
            {
              if (!SAFE_MUL_F32 (&acc, acc, 10.0f))
                {
                  goto failed;
                }
            }
        }
    }

  ASSERT (i == len); /* no extra characters */

  if (neg)
    acc = -acc;
  *dest = acc;
  return SUCCESS;

failed:
  return error_causef (
      e, ERR_ARITH,
      "Parse F32: Arithmetic Exception");
}

#define EPSILON 1e-6f

#ifndef NTEST
TEST (TT_UNIT, parse_f32_expect)
{
  f32 out = NAN;
  error e = error_create ();

  const struct string s1 = {
    .data = "3.14",
    .len = i_strlen ("3.14"),
  };
  test_assert_int_equal (parse_f32_expect (&out, s1, &e), SUCCESS);
  test_assert_int_equal (fabsf (out - 3.14f) < EPSILON, true);

  const struct string s2 = {
    .data = "-0.5",
    .len = i_strlen ("-0.5"),
  };
  test_assert_int_equal (parse_f32_expect (&out, s2, &e), SUCCESS);
  test_assert_int_equal (fabsf (out + 0.5f) < EPSILON, true);

  const struct string s3 = {
    .data = "1.23e3",
    .len = i_strlen ("1.23e3"),
  };
  test_assert_int_equal (parse_f32_expect (&out, s3, &e), SUCCESS);
  test_assert_int_equal (fabsf (out - 1230.0f) < EPSILON, true);

  const struct string s4 = {
    .data = ".25",
    .len = i_strlen (".25"),
  };
  test_assert_int_equal (parse_f32_expect (&out, s4, &e), SUCCESS);
  test_assert_int_equal (fabsf (out - 0.25f) < EPSILON, true);

  const struct string s5 = {
    .data = "1e40",
    .len = i_strlen ("1e40"),
  };
  test_assert_int_equal (parse_f32_expect (&out, s5, &e), ERR_ARITH);
}
#endif

float
py_mod_f32 (float num, float denom)
{
  if (denom == 0.0f)
    {
      return INFINITY;
    }

  float rem = num - denom * (int)(num / denom);

  if ((rem < 0.0f && denom > 0.0f) || (rem > 0.0f && denom < 0.0f))
    {
      rem += denom;
    }

  return rem;
}

#ifndef NTEST
TEST (TT_UNIT, py_mod_f32)
{
  /* +num , +denom  (generic) */
  test_assert (py_mod_f32 (5.5f, 2.0f) == 1.5f);

  /* –num , +denom  (Python keeps denom’s sign) */
  test_assert (py_mod_f32 (-5.5f, 2.0f) == 0.5f);

  /* +num , –denom  */
  test_assert (py_mod_f32 (5.5f, -2.0f) == -0.5f);

  /* –num , –denom  */
  test_assert (py_mod_f32 (-5.5f, -2.0f) == -1.5f);

  /* exact multiple (remainder 0) */
  test_assert (py_mod_f32 (4.0f, 2.0f) == 0.0f);

  /* zero numerator */
  test_assert (py_mod_f32 (0.0f, 3.3f) == 0.0f);

  /* denominator 0 ⇒ +INF  (sentinel-style) */
  test_assert (isinf (py_mod_f32 (7.0f, 0.0f)));
}
#endif

i32
py_mod_i32 (i32 num, i32 denom)
{
  i32 r = num % denom;
  if ((r != 0) && ((r < 0) != (denom < 0)))
    {
      r += denom;
    }
  return r;
}

#ifndef NTEST
TEST (TT_UNIT, py_mod_i32)
{
  /* +num , +denom  */
  test_assert (py_mod_i32 (5, 3) == 2);

  /* –num , +denom  */
  test_assert (py_mod_i32 (-5, 3) == 1);

  /* +num , –denom  */
  test_assert (py_mod_i32 (5, -3) == -1);

  /* –num , –denom  */
  test_assert (py_mod_i32 (-5, -3) == -2);

  /* exact multiple (remainder 0) */
  test_assert (py_mod_i32 (9, 3) == 0);

  /* zero numerator */
  test_assert (py_mod_i32 (0, 7) == 0);
}
#endif
