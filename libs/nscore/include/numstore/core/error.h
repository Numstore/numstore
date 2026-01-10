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
 *   Error handling framework with error codes, formatted error messages, and
 *   error propagation utilities. Supports error transformation and logging.
 */

// core
#include <numstore/core/macros.h>
#include <numstore/intf/types.h>

// MSVC doesn't support __attribute__, define it as empty
#ifdef _MSC_VER
#define __attribute__(x)
#endif

// #define ERR_T_FAIL_FAST

typedef enum
{
  SUCCESS = 0,

  // Core errors
  ERR_IO = -1,      // Generic IO
  ERR_CORRUPT = -2, // Corruption of disk data
  ERR_NOMEM = -3,   // No memory available
  ERR_ARITH = -4,   // Arithmetic exception

  // Pager Errors
  ERR_PAGER_FULL = -5,      // No more room in the pager
  ERR_TXN_FULL = -6,        // Transaction table is full
  ERR_DPGT_FULL = -7,       // Dirty page table is full
  ERR_VLOCKT_FULL = -8,     // Lock table is full
  ERR_PG_OUT_OF_RANGE = -9, // Requested page is out of range of available pages
  ERR_NO_TXN = -10,         // No transaction with the specified id

  // Compiler errors
  ERR_SYNTAX = -11, // Invalid syntax
  ERR_INTERP = -12, // Invalid semantics

  // Cursor errors
  ERR_RPTREE_PAGE_STACK_OVERFLOW = -13, // Stack overflow on pager
  ERR_RPTREE_INVALID = -14,             // Invalid rptree
  ERR_CURSORS_OVERFLOW = -15,           // Cursor table is full

  // Application Logic
  ERR_DUPLICATE_VARIABLE = -16,
  ERR_VARIABLE_NE = -17,

  // User Errors
  ERR_INVALID_ARGUMENT = -18,
  ERR_DUPLICATE_COMMIT = -19,
#ifndef NTEST
  ERR_FAILED_TEST = -29,
#endif
} err_t;

const char *err_t_to_str (err_t e);

typedef struct
{
  err_t cause_code;
  char cause_msg[256];
  u32 cmlen;
  bool print_trace;
  bool print_msg_on_error;
} error;

////////////////////////////////////////////////////////////
// /
/// Main API
error error_create (void);

err_t error_causef (
    error *e,
    err_t c,
    const char *fmt, ...)
    __attribute__ ((format (printf, 3, 4)));

err_t error_change_causef (
    error *e,
    err_t c,
    const char *fmt, ...)
    __attribute__ ((format (printf, 3, 4)));

err_t error_change_causef_from (
    error *e,
    err_t from,
    err_t to,
    const char *fmt, ...)
    __attribute__ ((format (printf, 4, 5)));

void error_log_consume (error *e);

bool error_equal (const error *left, const error *right);

////////////////////////////////////////////////////////////
// /
/// Macro Wrappers

#define error_reset(e)           \
  do                             \
    {                            \
      (e)->cause_code = SUCCESS; \
      (e)->cmlen = 0;            \
    }                            \
  while (0)

#define err_t_wrap(expr, e)           \
  do                                  \
    {                                 \
      err_t __ret = (err_t)expr;      \
      if (unlikely (__ret < SUCCESS)) \
        {                             \
          return (e)->cause_code;     \
        }                             \
    }                                 \
  while (0)

#define err_t_panic(expr, e)           \
  do                                   \
    {                                  \
      err_t __ret = (err_t)expr;       \
      if (unlikely (__ret < SUCCESS))  \
        {                              \
          error_log_consume (e);       \
          panic ("Panicing on error"); \
        }                              \
    }                                  \
  while (0)

#define err_t_panic(expr, e)           \
  do                                   \
    {                                  \
      err_t __ret = (err_t)expr;       \
      if (unlikely (__ret < SUCCESS))  \
        {                              \
          error_log_consume (e);       \
          panic ("Panicing on error"); \
        }                              \
    }                                  \
  while (0)

#define err_t_wrap_goto(expr, label, e) \
  do                                    \
    {                                   \
      err_t __ret = (err_t)expr;        \
      if (unlikely (__ret < SUCCESS))   \
        {                               \
          goto label;                   \
        }                               \
    }                                   \
  while (0)

#define err_t_wrap_null_goto(expr, label, e) \
  do                                         \
    {                                        \
      if (unlikely ((expr) == NULL))         \
        {                                    \
          goto label;                        \
        }                                    \
    }                                        \
  while (0)
