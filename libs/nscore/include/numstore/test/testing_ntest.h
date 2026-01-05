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
 *   No-op testing macros that compile to nothing, allowing test code to remain
 *   in source files while being completely eliminated when NTEST is defined.
 */

#define TEST_INIT_FOR_DLL(dll_name)
#define TEST(type, name)                                          \
  static inline void test_##name (void) __attribute__ ((unused)); \
  static inline void test_##name (void)
#define TEST_disabled(type, name)                                 \
  static inline void test_##name (void) __attribute__ ((unused)); \
  static inline void test_##name (void)
#define TEST_DISABLED(name, type)                                 \
  static inline void test_##name (void) __attribute__ ((unused)); \
  static inline void test_##name (void)
#define TEST_CASE(fmt, ...) if (0)
#define TEST_MARKER(_label) ((void)0)
#define CONDITIONAL_TEST_MARKER(condition, _label) ((void)0)
#define test_assert(expr) ((void)0)
#define test_assert_equal(left, right) ((void)0)
#define test_assert_nequal(left, right) ((void)0)
#define test_assert_int_equal(left, right) ((void)0)
#define test_assert_type_equal(left, right, type, fmt) ((void)0)
#define test_assert_ptr_equal(left, right) ((void)0)
#define test_assert_ptr_nequal(left, right) ((void)0)
#define test_assert_more(expr, fmt, ...) ((void)0)
#define test_assert_memequal(a, b, size) ((void)0)
#define test_fail_if(expr) ((void)0)
#define test_fail_if_null(expr) ((void)0)
#define test_err_t_wrap(expr, ename) ((void)(expr))
#define test_err_t_wrap_fatal(expr) ((void)(expr))
#define test_err_t_check(expr, exp, ename) ((void)(expr))
#define fail_test(fmt, ...) ((void)0)
#define fail_test_fatal(fmt, ...) ((void)0)
#define test_assert_local_coverage_hit(label) ((void)0)
#define test_assert_local_coverage_not_hit(label) ((void)0)
#define test_assert_global_coverage_hit(label) ((void)0)
#define test_assert_global_coverage_not_hit(label) ((void)0)

#define RANDOM_TEST(ttype, test_name, test_weight)                \
  static inline void test_##name (void) __attribute__ ((unused)); \
  static inline void test_##name (void)
#define TEST_AGENT(agentw, fmt, ...) if (0)

// Stub out coverage functions
static inline void
global_coverage_log (const char *label)
{
  (void)label;
}
