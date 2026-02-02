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
 *   Full testing framework with test registration, assertions, coverage tracking,
 *   random testing support, and detailed failure reporting for unit and profile tests.
 */

// core
#include <numstore/core/assert.h>
#include <numstore/core/filenames.h>
#include <numstore/core/signatures.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/stdlib.h>
#include <numstore/intf/types.h>

#define FPREFIX_STR "%s:%d (%s): \n            "
#define FPREFIX_ARGS basename (__FILE__), __LINE__, __func__

enum test_type
{
  TT_UNIT = (1 << 0),
  TT_PROFILE = (1 << 1),
  TT_HEAVY = (1 << 2),
};

typedef bool (*test_func) (void);
typedef struct
{
  test_func test;
  char *test_name;
  enum test_type tt;
} test;

typedef struct
{
  const char *file;
  int line;
  const char *func;
  const char *label;
  u64 index;
} coverage_point_info;

typedef struct
{
  bool was_hit;
} coverage_point_state;

// CONSTANTS - Global coverage tracking
extern coverage_point_info coverage_registry[2048];
extern coverage_point_state local_coverage_state[2048];
extern coverage_point_state global_coverage_state[2048];
extern u64 n_coverage_points;
extern double randt_unit;

extern u64 ntests;
extern test tests[2048];
extern int test_ret;
#define test_ntests ntests
#define test_tests tests
#define test_test_ret test_ret

#define TEST_disabled(type, name) \
  HEADER_FUNC void test_##name (void)

#define TEST_DISABLED(name, type) HEADER_FUNC void test_##name (void)

HEADER_FUNC void
reset_local_coverage (void)
{
  for (u64 i = 0; i < n_coverage_points; i++)
    local_coverage_state[i].was_hit = 0;
}

#define TEST(type, name)                                             \
  static void test_##name (void);                                    \
  static bool wrapper_test_##name (void)                             \
  {                                                                  \
    i_log_info ("========================= TEST CASE: %s\n", #name); \
    int prev = test_test_ret;                                        \
    test_test_ret = 0;                                               \
    reset_local_coverage ();                                         \
    test_##name ();                                                  \
    if (!test_test_ret)                                              \
      {                                                              \
        i_log_passed ("%s\n", #name);                                \
        test_test_ret = prev;                                        \
        return true;                                                 \
      }                                                              \
    return false;                                                    \
  }                                                                  \
  __attribute__ ((constructor)) static void register_##name (void)   \
  {                                                                  \
    test next = {                                                    \
      .test = wrapper_test_##name,                                   \
      .test_name = #name,                                            \
      .tt = type,                                                    \
    };                                                               \
    test_tests[test_ntests++] = next;                                \
  }                                                                  \
  static void test_##name (void)

#ifndef NLOGGING
#define TEST_CASE(fmt, ...)                                                                              \
  for (                                                                                                  \
      int _tc_once = (reset_local_coverage (), i_log_info ("------ CASE: " fmt "\n", ##__VA_ARGS__), 1), \
          _tc_prev = test_test_ret;                                                                      \
      _tc_once;                                                                                          \
      _tc_once = 0,                                                                                      \
          (test_test_ret == _tc_prev                                                                     \
               ? (i_log_passed ("------ : " fmt "\n", ##__VA_ARGS__), 0)                                 \
               : (i_log_failure ("------ : " fmt "\n", ##__VA_ARGS__), 0)))
#else
#define TEST_CASE(fmt, ...)                                                  \
  for (                                                                      \
      int _tc_once = (reset_local_coverage (), 1), _tc_prev = test_test_ret; \
      _tc_once;                                                              \
      _tc_once = 0)
#endif

#define CONDITIONAL_TEST_MARKER(condition, _label) \
  do                                               \
    {                                              \
      if (condition)                               \
        {                                          \
          TEST_MARKER (_label);                    \
        }                                          \
    }                                              \
  while (0)

#define TEST_MARKER(_label)                                               \
  do                                                                      \
    {                                                                     \
      static u64 _cp_idx = UINT64_MAX;                                    \
      if (_cp_idx == UINT64_MAX)                                          \
        {                                                                 \
          _cp_idx = n_coverage_points;                                    \
          coverage_registry[n_coverage_points++] = (coverage_point_info){ \
            .file = __FILE__,                                             \
            .line = __LINE__,                                             \
            .func = __func__,                                             \
            .label = _label,                                              \
            .index = _cp_idx                                              \
          };                                                              \
        }                                                                 \
      local_coverage_state[_cp_idx].was_hit = true;                       \
      global_coverage_state[_cp_idx].was_hit = true;                      \
    }                                                                     \
  while (0)

#define fail_test(fmt, ...)                                       \
  do                                                              \
    {                                                             \
      i_log_failure (FPREFIX_STR fmt, FPREFIX_ARGS, __VA_ARGS__); \
      test_test_ret = -1;                                         \
      return;                                                     \
    }                                                             \
  while (0)

#define fail_test_fatal(fmt, ...)                                 \
  do                                                              \
    {                                                             \
      i_log_failure (FPREFIX_STR fmt, FPREFIX_ARGS, __VA_ARGS__); \
      test_test_ret = -1;                                         \
      crash ();                                                   \
    }                                                             \
  while (0)

#define test_assert_equal(left, right)             \
  do                                               \
    {                                              \
      if ((left) != (right))                       \
        {                                          \
          fail_test ("%s != %s\n", #left, #right); \
        }                                          \
    }                                              \
  while (0)

#define test_assert_nequal(left, right)            \
  do                                               \
    {                                              \
      if ((left) == (right))                       \
        {                                          \
          fail_test ("%s != %s\n", #left, #right); \
        }                                          \
    }                                              \
  while (0)

#define test_assert_int_equal(left, right) test_assert_type_equal (left, right, i32, PRId32)

#define test_assert_type_equal(left, right, type, fmt)                          \
  do                                                                            \
    {                                                                           \
      type _left = left;                                                        \
      type _right = right;                                                      \
      if ((_left) != (_right))                                                  \
        {                                                                       \
          fail_test ("Expression: %s != %s values: (%" fmt ") != (%" fmt ")\n", \
                     #left, #right, _left, _right);                             \
        }                                                                       \
    }                                                                           \
  while (0)

#define test_err_t_wrap(expr, ename)                    \
  do                                                    \
    {                                                   \
      (ename)->print_trace = true;                      \
      err_t __ret = (err_t) (expr);                     \
      if ((__ret) < SUCCESS)                            \
        {                                               \
          fail_test ("Expression: %s failed\n", #expr); \
        }                                               \
      (ename)->print_trace = false;                     \
    }                                                   \
  while (0)

#define test_assert_ptr_equal(left, right) test_assert_equal ((void *)left, (void *)right)

#define test_assert_ptr_nequal(left, right) test_assert_nequal ((void *)left, (void *)right)

#define test_assert(expr)                      \
  do                                           \
    {                                          \
      if (!(expr))                             \
        {                                      \
          fail_test ("Expected: %s\n", #expr); \
        }                                      \
    }                                          \
  while (0)

#define test_assert_more(expr, fmt, ...)                               \
  do                                                                   \
    {                                                                  \
      if (!(expr))                                                     \
        {                                                              \
          fail_test ("Expected: %s\n            Context: (" fmt ")\n", \
                     #expr, __VA_ARGS__);                              \
        }                                                              \
    }                                                                  \
  while (0)

#define test_fail_if(expr)                       \
  do                                             \
    {                                            \
      if (expr)                                  \
        {                                        \
          fail_test ("Unexpected: %s\n", #expr); \
        }                                        \
    }                                            \
  while (0)

#define test_fail_if_null(expr)                           \
  do                                                      \
    {                                                     \
      if (expr == NULL)                                   \
        {                                                 \
          fail_test ("Expression: %s was null\n", #expr); \
        }                                                 \
    }                                                     \
  while (0)

#define test_err_t_wrap_fatal(expr)                                        \
  do                                                                       \
    {                                                                      \
      err_t __ret = (err_t)expr;                                           \
      if (__ret < SUCCESS)                                                 \
        {                                                                  \
          fail_test_fatal ("Expression: %s failed. Aborting...\n", #expr); \
        }                                                                  \
    }                                                                      \
  while (0)

#define test_err_t_check(expr, exp, ename) \
  do                                       \
    {                                      \
      err_t __ret = (err_t)expr;           \
      test_assert_int_equal (__ret, exp);  \
      (ename)->cause_code = SUCCESS;       \
    }                                      \
  while (0)

#define test_assert_memequal(a, b, size) test_assert_int_equal (memcmp (a, b, size), 0)
#define test_assert_str_equal(a, b)                       \
  do                                                      \
    {                                                     \
      test_assert_int_equal (i_strlen (a), i_strlen (b)); \
      test_assert_memequal (a, b, i_strlen (b));          \
    }                                                     \
  while (0)

HEADER_FUNC bool
local_coverage_hit (const char *label)
{
  coverage_point_info *hit = NULL;
  for (u64 _i = 0; _i < n_coverage_points; _i++)
    {
      if (coverage_registry[_i].label != NULL && strcmp (coverage_registry[_i].label, label) == 0)
        {
          hit = &coverage_registry[_i];
          break;
        }
    }
  if (hit == NULL)
    {
      i_log_failure ("Coverage point '%s' was not registered, and therefore not hit\n", label);
      return false;
    }
  else if (!local_coverage_state[hit->index].was_hit)
    {
      i_log_failure ("Coverage point '%s' at %s:%d was not hit\n", label, hit->file, hit->line);
      return false;
    }

  return true;
}

HEADER_FUNC bool
local_coverage_not_hit (const char *label)
{
  coverage_point_info *hit = NULL;
  for (u64 _i = 0; _i < n_coverage_points; _i++)
    {
      if (coverage_registry[_i].label != NULL && strcmp (coverage_registry[_i].label, label) == 0)
        {
          hit = &coverage_registry[_i];
          break;
        }
    }
  if (hit == NULL)
    {
      return true;
    }
  else if (local_coverage_state[hit->index].was_hit)
    {
      i_log_failure ("Coverage point '%s' at %s:%d was hit\n", label, hit->file, hit->line);
      return false;
    }

  return true;
}

HEADER_FUNC bool
global_coverage_hit (const char *label)
{
  coverage_point_info *hit = NULL;
  for (u64 _i = 0; _i < n_coverage_points; _i++)
    {
      if (coverage_registry[_i].label != NULL && strcmp (coverage_registry[_i].label, label) == 0)
        {
          hit = &coverage_registry[_i];
          break;
        }
    }
  if (hit == NULL)
    {
      i_log_failure ("Coverage point '%s' was not registered, and therefore not hit\n", label);
      return false;
    }
  else if (!global_coverage_state[hit->index].was_hit)
    {
      UNREACHABLE (); // Registering means it was hit
    }

  return true;
}

HEADER_FUNC bool
global_coverage_not_hit (const char *label)
{
  coverage_point_info *hit = NULL;
  for (u64 _i = 0; _i < n_coverage_points; _i++)
    {
      if (coverage_registry[_i].label != NULL && strcmp (coverage_registry[_i].label, label) == 0)
        {
          hit = &coverage_registry[_i];
          break;
        }
    }
  if (hit == NULL)
    {
      return true;
    }
  else if (global_coverage_state[hit->index].was_hit)
    {
      i_log_failure ("Coverage point '%s' at %s:%d was hit\n", label, hit->file, hit->line);
      return false;
    }

  return true;
}

HEADER_FUNC void
global_coverage_log (const char *label)
{
  coverage_point_info *hit = NULL;
  for (u64 _i = 0; _i < n_coverage_points; _i++)
    {
      if (coverage_registry[_i].label != NULL && strcmp (coverage_registry[_i].label, label) == 0)
        {
          hit = &coverage_registry[_i];
          break;
        }
    }
  if (hit == NULL)
    {
      i_log_warn ("Coverage point '%s' was not registered, and therefore not hit\n", label);
    }
  else if (global_coverage_state[hit->index].was_hit)
    {
      i_log_passed ("Coverage point '%s' at %s:%d was hit\n", label, hit->file, hit->line);
    }
  else
    {
      UNREACHABLE ();
    }
}

#define test_assert_local_coverage_hit(label)   \
  do                                            \
    {                                           \
      test_assert (local_coverage_hit (label)); \
    }                                           \
  while (0)

#define test_assert_local_coverage_not_hit(label)   \
  do                                                \
    {                                               \
      test_assert (local_coverage_not_hit (label)); \
    }                                               \
  while (0)

#define test_assert_global_coverage_hit(label)   \
  do                                             \
    {                                            \
      test_assert (global_coverage_hit (label)); \
    }                                            \
  while (0)

#define test_assert_global_coverage_not_hit(label)   \
  do                                                 \
    {                                                \
      test_assert (global_coverage_not_hit (label)); \
    }                                                \
  while (0)

//////////////////////////////////////////////////////
/// RANDOM TEST

#ifndef RANDOM_TEST_DEFAULT_UNIT_SECONDS
#define RANDOM_TEST_DEFAULT_UNIT_SECONDS 100
#endif

HEADER_FUNC void
randt_set_unit (double seconds)
{
  randt_unit = seconds;
}

HEADER_FUNC double
randt_get_unit (void)
{
  return randt_unit;
}

HEADER_FUNC bool
randt_check_time (struct timespec *start, double dur)
{
  if (start->tv_sec == 0 && start->tv_nsec == 0)
    {
      i_get_monotonic_time (start);
      return true;
    }

  struct timespec now;
  i_get_monotonic_time (&now);

  double elapsed = (now.tv_sec - start->tv_sec) + (now.tv_nsec - start->tv_nsec) / 1e9;

  return elapsed < dur;
}

#define RANDOM_TEST(ttype, test_name, test_weight)     \
  static void test_name##_impl (double _randt_weight); \
  TEST (ttype, test_name)                              \
  {                                                    \
    double _randt_weight = test_weight;                \
    test_name##_impl (_randt_weight);                  \
  }                                                    \
  static void test_name##_impl (double testw)

#define TEST_AGENT(agentw, fmt, ...) \
  for (struct timespec t = (i_log_info ("------ CASE: " fmt "\n", ##__VA_ARGS__), (struct timespec){ 0 }); randt_check_time (&t, (agentw)*testw * randt_get_unit ());)
