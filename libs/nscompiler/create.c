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
 *   Implements queries/create.h. Provides builder pattern implementation for
 *   constructing CREATE queries that define new variables with types.
 */

#include <numstore/compiler/queries/create.h>

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/core/strings_utils.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/test/testing.h>

// core
DEFINE_DBG_ASSERT (
    struct create_query, create_query, q,
    {
      ASSERT (q);
    })

DEFINE_DBG_ASSERT (
    struct create_builder, create_builder, c,
    {
      ASSERT (c);
    })

void
i_log_create (struct create_query *q)
{
  DBG_ASSERT (create_query, q);
  int n = type_snprintf (NULL, 0, &q->type);

  i_log_info ("Creating variable with name: %.*s\n",
              q->vname.len, q->vname.data);

  error e = error_create ();
  char *str = i_malloc (n + 1, 1, &e);
  if (str != NULL)
    {
      type_snprintf (str, n + 1, &q->type);
      i_log_info ("Variable Type: %.*s\n", n, str);
      i_free (str);
    }
  else
    {
      error_log_consume (&e);
      i_log_info ("Variable Type: (Omitted due to length)\n");
    }
}

bool
create_query_equal (const struct create_query *left, const struct create_query *right)
{
  DBG_ASSERT (create_query, left);
  DBG_ASSERT (create_query, right);

  if (!string_equal (left->vname, right->vname))
    {
      return false;
    }
  if (!type_equal (&left->type, &right->type))
    {
      return false;
    }
  return true;
}

struct create_builder
crb_create (void)
{
  struct create_builder ret = {
    .type = (struct type){ 0 },
    .vname = (struct string){ 0 },
    .got_type = false,
    .got_vname = false,
  };

  DBG_ASSERT (create_builder, &ret);

  return ret;
}

err_t
crb_accept_string (struct create_builder *b, const struct string vname, error *e)
{
  DBG_ASSERT (create_builder, b);
  if (b->got_vname)
    {
      return error_causef (
          e, ERR_INTERP,
          "Create Builder: "
          "Already have a variable name");
    }

  if (vname.len == 0)
    {
      return error_causef (
          e, ERR_INTERP,
          "Create Builder: "
          "Length of variable names must be > 0");
    }

  b->vname = vname;
  b->got_vname = true;

  return SUCCESS;
}

err_t
crb_accept_type (struct create_builder *b, struct type t, error *e)
{
  DBG_ASSERT (create_builder, b);
  if (b->got_type)
    {
      return error_causef (
          e, ERR_INTERP,
          "Create Builder: "
          "Already have a type literal");
    }

  b->type = t;
  b->got_type = true;

  return SUCCESS;
}

err_t
crb_build (struct create_query *dest, struct create_builder *b, error *e)
{
  DBG_ASSERT (create_builder, b);
  ASSERT (dest);

  if (!b->got_type)
    {
      return error_causef (
          e, ERR_INTERP,
          "Create Builder: "
          "Need a type to build a create query");
    }

  if (!b->got_vname)
    {
      return error_causef (
          e, ERR_INTERP,
          "Create Builder: "
          "Need a variable name to build a create query");
    }

  dest->type = b->type;
  dest->vname = b->vname;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, create_builder)
{
  error err = error_create ();

  /* 0. freshly‑created builder must be clean */
  struct create_builder b = crb_create ();
  test_fail_if (b.got_vname);
  test_fail_if (b.got_type);

  /* 1. rejecting empty variable names */
  test_assert_int_equal (crb_accept_string (&b, (struct string){ 0 }, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 2. first, accept a variable name */
  struct string name = strfcstr ("foo");
  test_assert_int_equal (crb_accept_string (&b, name, &err), SUCCESS);
  test_fail_if (!b.got_vname);

  /* 3. duplicate name must fail */
  test_assert_int_equal (crb_accept_string (&b, name, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 4. accept a type */
  struct type t = (struct type){ .type = T_PRIM };
  test_assert_int_equal (crb_accept_type (&b, t, &err), SUCCESS);
  test_fail_if (!b.got_type);

  /* 5. duplicate type must fail */
  test_assert_int_equal (crb_accept_type (&b, t, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 6. build now that both pieces are present */
  struct create_query q = { 0 };
  test_assert_int_equal (crb_build (&q, &b, &err), SUCCESS);
  test_fail_if (q.type.type != t.type);
  test_fail_if (q.vname.len != name.len);

  /* 7. build with *missing type* must fail */
  struct create_builder missing_type = crb_create ();
  test_assert_int_equal (crb_accept_string (&missing_type, name, &err), SUCCESS);
  test_assert_int_equal (crb_build (&q, &missing_type, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 8. build with *missing vname* must fail */
  struct create_builder missing_name = crb_create ();
  test_assert_int_equal (crb_accept_type (&missing_name, t, &err), SUCCESS);
  test_assert_int_equal (crb_build (&q, &missing_name, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 9. order‑independence: type first, then name */
  struct create_builder reorder = crb_create ();
  test_assert_int_equal (crb_accept_type (&reorder, t, &err), SUCCESS);
  test_assert_int_equal (crb_accept_string (&reorder, name, &err), SUCCESS);
  test_assert_int_equal (crb_build (&q, &reorder, &err), SUCCESS);
}
#endif
