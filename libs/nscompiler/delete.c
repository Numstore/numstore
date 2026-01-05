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
 *   Implements queries/delete.h. Provides builder pattern implementation for
 *   constructing DELETE queries that remove variables.
 */

#include <numstore/compiler/queries/delete.h>

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/core/strings_utils.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/test/testing.h>

// core
DEFINE_DBG_ASSERT (
    struct delete_query, delete_query, q,
    {
      ASSERT (q);
    })

DEFINE_DBG_ASSERT (
    struct delete_builder, delete_builder, c,
    {
      ASSERT (c);
    })

void
i_log_delete (struct delete_query *q)
{
  DBG_ASSERT (delete_query, q);
  i_log_info ("delete %.*s\n", q->vname.len, q->vname.data);
}

bool
delete_query_equal (const struct delete_query *left, const struct delete_query *right)
{
  DBG_ASSERT (delete_query, left);
  DBG_ASSERT (delete_query, right);

  if (!string_equal (left->vname, right->vname))
    {
      return false;
    }

  return true;
}
struct delete_builder
dltb_create (void)
{
  struct delete_builder ret = {
    .vname = { 0 },
    .got_vname = false,
  };

  DBG_ASSERT (delete_builder, &ret);

  return ret;
}

err_t
dltb_accept_string (struct delete_builder *b, const struct string vname, error *e)
{
  DBG_ASSERT (delete_builder, b);
  if (b->got_vname)
    {
      return error_causef (
          e, ERR_INTERP,
          "Delete Builder: "
          "Already have a variable name");
    }

  if (vname.len == 0)
    {
      return error_causef (
          e, ERR_INTERP,
          "Delete Builder: "
          "Length of variable names must be > 0");
    }

  b->vname = vname;
  b->got_vname = true;

  return SUCCESS;
}

err_t
dltb_build (struct delete_query *dest, struct delete_builder *b, error *e)
{
  DBG_ASSERT (delete_builder, b);
  ASSERT (dest);

  if (!b->got_vname)
    {
      return error_causef (
          e, ERR_INTERP,
          "Delete Builder: "
          "Need a variable name to build a delete query");
    }

  dest->vname = b->vname;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, delete_builder)
{
  error err = error_create ();

  /* 0. freshly‑created builder must be clean */
  struct delete_builder b = dltb_create ();
  test_fail_if (b.got_vname);

  /* 1. rejecting empty variable names */
  test_assert_int_equal (dltb_accept_string (&b, (struct string){ 0 }, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 2. first, accept a variable name */
  struct string name = strfcstr ("foo");
  test_assert_int_equal (dltb_accept_string (&b, name, &err), SUCCESS);
  test_fail_if (!b.got_vname);

  /* 3. duplicate name must fail */
  test_assert_int_equal (dltb_accept_string (&b, name, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 4. build now that the piece is present */
  struct delete_query q = { 0 };
  test_assert_int_equal (dltb_build (&q, &b, &err), SUCCESS);
  test_fail_if (q.vname.len != name.len);

  /* 5. build with *missing vname* must fail */
  struct delete_builder missing_name = dltb_create ();
  test_assert_int_equal (dltb_build (&q, &missing_name, &err), ERR_INTERP);
  err.cause_code = SUCCESS;
}
#endif
