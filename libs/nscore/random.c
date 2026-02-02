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
 *   Implements random.h. Random number generation for all integer types and utilities.
 */

#include "numstore/core/chunk_alloc.h"
#include <numstore/core/random.h>

#include <numstore/core/assert.h>
#include <numstore/intf/types.h>
#include <numstore/test/testing.h>

#include <stdlib.h>
#include <time.h>

#ifdef _MSC_VER
#include <intrin.h>
#endif

void
rand_seed (void)
{
  srand (time (NULL));
}

void
rand_seed_with (u32 seed)
{
  srand (seed);
}

u8
randu8 (void)
{
  return (u8)rand ();
}

i8
randi8 (void)
{
  return randi8r (I8_MIN, I8_MAX);
}

u8
randu8r (u8 lower, u8 upper)
{
  return (u32)randu32r ((u32)lower, (u32)upper);
}

i8
randi8r (i8 lower, i8 upper)
{
  return (i8)randi32r ((i32)lower, (i32)upper);
}

u16
randu16 (void)
{
  return (u16)randu32 ();
}

i16
randi16 (void)
{
  return randi16r (I16_MIN, I16_MAX);
}

u16
randu16r (u16 lower, u16 upper)
{
  return (u16)randu32r ((u32)lower, (u32)upper);
}

i16
randi16r (i16 lower, i16 upper)
{
  ASSERT (upper >= lower);

  if (upper == lower)
    {
      return lower;
    }

  return (i16)randi32r ((i32)lower, (i32)upper);
}

u32
randu32 (void)
{
  return (u32)rand ();
}

#ifndef NTEST
TEST (TT_UNIT, randu32)
{
  srand (0);
  for (int i = 0; i < 1000; ++i)
    {
      u32 v = randu32 ();
      test_assert (v <= (u32)RAND_MAX);
    }
}
#endif

i32
randi32 (void)
{
  return randi32r (I32_MIN, I32_MAX);
}

u32
randu32r (u32 lower, u32 upper)
{
  ASSERT (upper >= lower);

  if (upper == lower)
    {
      return lower;
    }

  u32 r = (u32)((upper - lower) * ((f32)rand () / (f32)RAND_MAX));

  return lower + r;
}

#ifndef NTEST
TEST (TT_UNIT, randu32r)
{
  srand (1);
  /* lower == upper */
  test_assert_type_equal (randu32r (5, 5), 5u, u32, "ud");
  test_assert_type_equal (randu32r (0, 0), 0u, u32, "ud");
  test_assert_type_equal (randu32r (U32_MAX, U32_MAX), U32_MAX, u32, "ud");

  /* tiny range */
  for (int i = 0; i < 10; ++i)
    {
      u32 v = randu32r (10, 11);
      test_assert (v == 10u);
    }

  /* full 32-bit range */
  for (int i = 0; i < 10; ++i)
    {
      u32 v = randu32r (0u, U32_MAX);
      test_assert (v <= U32_MAX);
    }

  /* random ranges */
  for (int i = 0; i < 10; ++i)
    {
      u32 lo = (u32)(rand () % 10000);
      u32 hi = lo + (u32)(rand () % 10000);
      u32 v = randu32r (lo, hi);
      test_assert (v >= lo);
      test_assert (v <= hi);
    }
}
#endif

i32
randi32r (i32 lower, i32 upper)
{
  ASSERT (upper >= lower);

  if (upper == lower)
    {
      return lower;
    }

  u64 range = (u64)((int64_t)upper - (int64_t)lower + 1);
  u64 r = randu64r (0, range - 1);
  return lower + (i32)r;
}

#ifndef NTEST
TEST (TT_UNIT, randi32r)
{
  rand_seed_with (4);
  test_assert_int_equal (randi32r (7, 7), 7);
  test_assert_int_equal (randi32r (I32_MIN, I32_MIN), I32_MIN);
  test_assert_int_equal (randi32r (I32_MAX, I32_MAX), I32_MAX);

  for (int i = 0; i < 100; ++i)
    {
      i32 v = randi32r (-10, -9);
      test_assert (v == -10 || v == -9);
    }

  for (int i = 0; i < 100; ++i)
    {
      i32 v = randi32r (I32_MIN, I32_MAX);
      test_assert (v >= I32_MIN);
      test_assert (v <= I32_MAX);
    }

  for (int i = 0; i < 100; ++i)
    {
      i32 lo = (i32)(rand () % 10000) - 5000;
      i32 hi = lo + (i32)(rand () % 10000);
      i32 v = randi32r (lo, hi);
      test_assert (v >= lo);
      test_assert (v <= hi);
    }
}
#endif

u64
randu64 (void)
{
  const u64 base = (u64)RAND_MAX + 1u;
  u64 r0 = (u64)randu32 ();
  u64 r1 = (u64)randu32 ();
  u64 r2 = (u64)randu32 ();
  u64 r3 = (u64)randu32 ();
  return (((r0 * base + r1) * base + r2) * base) + r3;
}

i64
randi64 (void)
{
  return randi64r (I64_MIN, I64_MAX);
}

u64
randu64r (u64 lower, u64 upper)
{
  ASSERT (upper >= lower);

  if (upper == lower)
    {
      return lower;
    }

#ifdef _MSC_VER
  // MSVC doesn't support __uint128_t, use _umul128 intrinsic
  u64 range = upper - lower + 1u;
  u64 rand_val = randu64 ();
  u64 high_bits;
  _umul128 (rand_val, range, &high_bits);
  return lower + high_bits;
#else
  __uint128_t range = (__uint128_t)upper - (__uint128_t)lower + 1u;
  __uint128_t prod = (__uint128_t)randu64 () * range;
  u64 r = (u64)(prod >> 64);
  return lower + r;
#endif
}

#ifndef NTEST
TEST (TT_UNIT, randu64r)
{
  srand (3);
  test_assert_type_equal (randu64r (1234567890123ull, 1234567890123ull), 1234567890123ull, u64, PRIu64);
  test_assert_type_equal (randu64r (0ull, 0ull), 0ull, u64, PRIu64);
  test_assert_type_equal (randu64r (U64_MAX, U64_MAX), U64_MAX, u64, PRIu64);

  for (int i = 0; i < 100; ++i)
    {
      u64 v = randu64r (1000ull, 1001ull);
      test_assert (v == 1000ull || v == 1001ull);
    }

  for (int i = 0; i < 100; ++i)
    {
      u64 v = randu64r (0ull, U64_MAX);
      test_assert (v <= U64_MAX);
    }

  for (int i = 0; i < 50; ++i)
    {
      u64 lo = ((u64)randu32 () << 16);
      u64 hi = lo + (u64)(randu32 () % 100000);
      u64 v = randu64r (lo, hi);
      test_assert (v >= lo);
      test_assert (v <= hi);
    }
}
#endif

i64
randi64r (i64 lower, i64 upper)
{
  ASSERT (upper >= lower);
  if (upper == lower)
    return lower;

#ifdef _MSC_VER
  // MSVC doesn't support __uint128_t, use _umul128 intrinsic
  // Cast to unsigned for multiplication, then cast back
  u64 range_u = (u64)((u64)upper - (u64)lower + 1u);
  u64 rand_val = randu64 ();
  u64 high_bits;
  _umul128 (rand_val, range_u, &high_bits);
  return lower + (i64)high_bits;
#else
  __uint128_t range = (__uint128_t)((__int128_t)upper - (__int128_t)lower + 1);
  __uint128_t prod = (__uint128_t)randu64 () * range;
  i64 r = (i64)(prod >> 64);
  return lower + r;
#endif
}

#ifndef NTEST
TEST (TT_UNIT, randi64r)
{
  srand (11);
  test_assert_type_equal (randi64r (7, 7), 7, i64, PRId64);
  test_assert_type_equal (randi64r (I64_MIN, I64_MIN), I64_MIN, i64, PRId64);
  test_assert_type_equal (randi64r (I64_MAX, I64_MAX), I64_MAX, i64, PRId64);
  for (int i = 0; i < 10; ++i)
    {
      i64 lo = (i64)(rand () % 100000) - 50000;
      i64 hi = lo + (i64)(rand () % 100000);
      i64 v = randi64r (lo, hi);
      test_assert (v >= lo && v <= hi);
    }
}
#endif

err_t
rand_str (struct string *dest, struct chunk_alloc *alloc, u32 minlen, u32 maxlen, error *e)
{
  ASSERT (dest);
  ASSERT (maxlen > minlen);

  u32 len = randu32r (minlen, maxlen);
  char *data = (char *)chunk_malloc (alloc, len, sizeof *data, e);
  if (!data)
    {
      return e->cause_code;
    }

  for (u32 i = 0; i < len; ++i)
    {
      /* Choose a printable ASCII character from [a-zA-Z0-9] */
      int r = randu32r (0, 62);
      if (r < 10)
        {
          data[i] = '0' + r;
        }
      else if (r < 36)
        {
          data[i] = 'A' + (r - 10);
        }
      else
        {
          data[i] = 'a' + (r - 36);
        }
    }

  dest->len = (u16)len;
  dest->data = data;

  return SUCCESS;
}

void
rand_bytes (void *dest, u32 len)
{
  ASSERT (dest);
  ASSERT (len > 0);

  u8 *p = (u8 *)dest;
  for (u32 i = 0; i < len; ++i)
    {
      p[i] = (u8)(rand () & 0xFF);
    }
}

void
shuffle_u32 (u32 *array, u32 len)
{
  for (int i = len - 1; i > 0; i--)
    {
      int j = randu32 () % (i + 1);

      // Swap array[i] with array[j]
      u32 temp = array[i];
      array[i] = array[j];
      array[j] = temp;
    }
}
