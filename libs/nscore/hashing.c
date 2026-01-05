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
 *   Implements hashing.h. FNV-1a hash functions for strings and data.
 */

#include <numstore/core/hashing.h>

#include <numstore/test/testing.h>

// core
u32
fnv1a_hash (const struct cstring s)
{
  u32 hash = 2166136261u;
  const char *str = s.data;
  for (u32 i = 0; i < s.len; ++i)
    {
      hash ^= (u8)*str++;
      hash *= 16777619u;
    }
  return hash;
}

#ifndef NTEST
TEST (TT_UNIT, fnv1a_hash_empty)
{
  struct cstring empty = { .data = "", .len = 0 };
  u32 hash = fnv1a_hash (empty);
  // Empty string should return FNV offset basis
  test_assert_equal (hash, 2166136261u);
}

TEST (TT_UNIT, fnv1a_hash_single_char)
{
  struct cstring s = { .data = "a", .len = 1 };
  u32 hash = fnv1a_hash (s);
  // "a" should hash to (2166136261 ^ 'a') * 16777619
  u32 expected = (2166136261u ^ 'a') * 16777619u;
  test_assert_equal (hash, expected);
}

TEST (TT_UNIT, fnv1a_hash_known_value)
{
  struct cstring s = { .data = "hello", .len = 5 };
  u32 hash = fnv1a_hash (s);
  // Known FNV-1a hash for "hello"
  test_assert_equal (hash, 1335831723u);
}

TEST (TT_UNIT, fnv1a_hash_deterministic)
{
  struct cstring s = { .data = "test", .len = 4 };
  u32 hash1 = fnv1a_hash (s);
  u32 hash2 = fnv1a_hash (s);
  // Same input should always produce same hash
  test_assert_equal (hash1, hash2);
}
#endif
