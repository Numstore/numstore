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
 *   Implements error.h. Error handling and propagation with stack traces.
 */

#include <numstore/core/error.h>

#include <numstore/core/assert.h>
#include <numstore/core/macros.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/intf/stdlib.h>
#include <numstore/test/testing.h>

#include <stdarg.h>
#include <stdlib.h>

// core
// system
#ifdef ERR_T_FAIL_FAST
#endif

DEFINE_DBG_ASSERT (
    error, error, e,
    {
      ASSERT (e);
      if (e->cause_code != SUCCESS)
        {
          ASSERT (e->cmlen > 0);
        }
    })

const char *
err_t_to_str (err_t e)
{
  switch (e)
    {
      case_ENUM_RETURN_STRING (SUCCESS);

      // Core errors
      case_ENUM_RETURN_STRING (ERR_IO);
      case_ENUM_RETURN_STRING (ERR_CORRUPT);
      case_ENUM_RETURN_STRING (ERR_NOMEM);
      case_ENUM_RETURN_STRING (ERR_ARITH);

      // Pager Errors
      case_ENUM_RETURN_STRING (ERR_PAGER_FULL);
      case_ENUM_RETURN_STRING (ERR_TXN_FULL);
      case_ENUM_RETURN_STRING (ERR_DPGT_FULL);
      case_ENUM_RETURN_STRING (ERR_VLOCKT_FULL);
      case_ENUM_RETURN_STRING (ERR_PG_OUT_OF_RANGE);
      case_ENUM_RETURN_STRING (ERR_NO_TXN);

      // Compiler errors
      case_ENUM_RETURN_STRING (ERR_SYNTAX);
      case_ENUM_RETURN_STRING (ERR_INTERP);

      // Cursor errors
      case_ENUM_RETURN_STRING (ERR_RPTREE_PAGE_STACK_OVERFLOW);
      case_ENUM_RETURN_STRING (ERR_RPTREE_INVALID);
      case_ENUM_RETURN_STRING (ERR_CURSORS_OVERFLOW);

      case_ENUM_RETURN_STRING (ERR_DUPLICATE_VARIABLE);
      case_ENUM_RETURN_STRING (ERR_VARIABLE_NE);

      case_ENUM_RETURN_STRING (ERR_INVALID_ARGUMENT);
      case_ENUM_RETURN_STRING (ERR_DUPLICATE_COMMIT);

      case_ENUM_RETURN_STRING (ERR_TOO_MANY_FILES);

#ifndef NTEST
      case_ENUM_RETURN_STRING (ERR_FAILED_TEST);
#endif
    }
  UNREACHABLE ();
}

error
error_create (void)
{
  error ret = {
    .cause_code = SUCCESS,
    .cmlen = 0,
    .print_trace = false,
    .print_msg_on_error = true,
  };

  DBG_ASSERT (error, &ret);

  return ret;
}

err_t
error_causef (error *e, err_t c, const char *fmt, ...)
{
  ASSERT (fmt);

  va_list ap;
  va_start (ap, fmt);

  char tmpbuf[256];
  va_list ap_copy;
  va_copy (ap_copy, ap);
  u32 cmlen = vsnprintf (tmpbuf, sizeof (tmpbuf), fmt, ap_copy);
  cmlen = MIN (cmlen, 255);
  va_end (ap_copy);

  if (e)
    {
      if (e->cause_code)
        {
          i_log_error ("SWALLOWING ERROR: %s\n", tmpbuf);
        }
      else
        {
          ASSERT (e->cause_code == SUCCESS);
          e->cause_code = c;
          memcpy (e->cause_msg, tmpbuf, cmlen);
          e->cause_msg[cmlen] = '\0';
          e->cmlen = cmlen;
        }

      if (e->print_msg_on_error)
        {
          i_log_error ("%.*s\n", e->cmlen, e->cause_msg);
        }

      if (e->print_trace)
        {
          i_print_stack_trace ();
        }
    }
  else
    {
      i_log_error ("%s\n", tmpbuf);
    }

  va_end (ap);

#ifdef ERR_T_FAIL_FAST
  abort ();
#endif

  return c;
}

err_t
error_change_causef_from (
    error *e,
    err_t from,
    err_t to,
    const char *fmt, ...)
{
  ASSERT (fmt);

  if (e)
    {
      DBG_ASSERT (error, e);
      ASSERT (e->cause_code == from);
      e->cause_code = to;

      /**
       * Print into cause message
       * (255 because of null terminator)
       */
      va_list ap;
      va_start (ap, fmt);

      va_list ap_copy;
      va_copy (ap_copy, ap);
      u32 cmlen = i_vsnprintf (e->cause_msg, 256, fmt, ap_copy);
      e->cmlen = MIN (cmlen, 255);
      va_end (ap_copy);
      va_end (ap);
    }

  i_log_error ("%.*s\n", e->cmlen, e->cause_msg);

  return to;
}

err_t
error_change_causef (
    error *e,
    err_t c,
    const char *fmt, ...)
{
  ASSERT (fmt);
  if (e)
    {
      DBG_ASSERT (error, e);
      ASSERT (e->cause_code != SUCCESS); /* Can only go from good -> cause */
      e->cause_code = c;

      /**
       * Print into cause message
       * (255 because of null terminator)
       */
      va_list ap;
      va_start (ap, fmt);

      va_list ap_copy;
      va_copy (ap_copy, ap);
      u32 cmlen = i_vsnprintf (e->cause_msg, 256, fmt, ap_copy);
      e->cmlen = MIN (cmlen, 255);
      va_end (ap_copy);
      va_end (ap);
    }

  i_log_error ("%.*s\n", e->cmlen, e->cause_msg);

  return c;
}

void
error_log_consume (error *e)
{
  DBG_ASSERT (error, e);
  ASSERT (e->cause_code != SUCCESS);
  i_log_error ("%.*s\n", e->cmlen, e->cause_msg);
  e->cmlen = 0;
  e->cause_code = SUCCESS;
}

bool
error_equal (const error *left, const error *right)
{
  DBG_ASSERT (error, left);
  DBG_ASSERT (error, right);
  if (left->cause_code != right->cause_code)
    {
      return false;
    }

  if (left->cause_code)
    {
      if (left->cmlen != right->cmlen)
        {
          return false;
        }
      if (i_strncmp (left->cause_msg, right->cause_msg, left->cmlen) != 0)
        {
          return false;
        }
    }

  return true;
}
