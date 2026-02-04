#include "numstore/core/stride.h"
#include <numstore/types/type_accessor.h>

#include <numstore/core/assert.h>
#include <numstore/test/testing.h>

bool
user_stride_equal (const struct user_stride *left, const struct user_stride *right)
{
  return left->start == right->start && left->step == right->step && left->stop == right->stop
         && left->present == right->present;
}

bool
type_accessor_equal (const struct type_accessor *left, const struct type_accessor *right)
{
  if (left->type != right->type)
    {
      return false;
    }

  switch (left->type)
    {
    case TA_TAKE:
      {
        return true;
      }
    case TA_SELECT:
      {
        if (!string_equal (left->select.key, right->select.key))
          {
            return false;
          }
        if (left->select.sub_ta == NULL && right->select.sub_ta == NULL)
          {
            return true;
          }
        if (left->select.sub_ta == NULL || right->select.sub_ta == NULL)
          {
            return false;
          }
        return type_accessor_equal (left->select.sub_ta, right->select.sub_ta);
      }
    case TA_RANGE:
      {
        if (!user_stride_equal (&left->range.stride, &right->range.stride))
          {
            return false;
          }
        if (left->range.sub_ta == NULL && right->range.sub_ta == NULL)
          {
            return true;
          }
        if (left->range.sub_ta == NULL || right->range.sub_ta == NULL)
          {
            return false;
          }
        return type_accessor_equal (left->range.sub_ta, right->range.sub_ta);
      }
    }

  return false;
}

DEFINE_DBG_ASSERT (
    struct type_accessor_builder, type_accessor_builder, s,
    {
      ASSERT (s);
    })

void
tab_create (
    struct type_accessor_builder *dest,
    struct chunk_alloc *persistent)
{
  *dest = (struct type_accessor_builder){
    .head = NULL,
    .tail = NULL,
    .persistent = persistent,
  };

  DBG_ASSERT (type_accessor_builder, dest);
}

err_t
tab_accept_select (struct type_accessor_builder *builder, struct string key, error *e)
{
  DBG_ASSERT (type_accessor_builder, builder);

  /* Allocate new accessor */
  struct type_accessor *ta;
  if (builder->head == NULL)
    {
      ta = &builder->ret;
    }
  else
    {
      ta = chunk_malloc (builder->persistent, 1, sizeof *ta, e);
      if (!ta)
        {
          return e->cause_code;
        }
    }

  /* Copy key to persistent memory */
  key.data = chunk_alloc_move_mem (builder->persistent, key.data, key.len, e);
  if (!key.data)
    {
      return e->cause_code;
    }

  /* Initialize select accessor */
  ta->type = TA_SELECT;
  ta->select.key = key;
  ta->select.sub_ta = NULL;

  /* Link into chain */
  if (!builder->head)
    {
      builder->head = ta;
      builder->tail = ta;
    }
  else
    {
      /* Need to set sub_ta based on previous type */
      if (builder->tail->type == TA_SELECT)
        {
          builder->tail->select.sub_ta = ta;
        }
      else if (builder->tail->type == TA_RANGE)
        {
          builder->tail->range.sub_ta = ta;
        }
      builder->tail = ta;
    }

  return SUCCESS;
}

err_t
tab_accept_range (
    struct type_accessor_builder *builder,
    struct user_stride stride,
    error *e)
{
  DBG_ASSERT (type_accessor_builder, builder);

  /* Allocate new accessor */
  struct type_accessor *ta;
  if (builder->head == NULL)
    {
      ta = &builder->ret;
    }
  else
    {
      ta = chunk_malloc (builder->persistent, 1, sizeof *ta, e);
      if (!ta)
        {
          return e->cause_code;
        }
    }

  /* Initialize range accessor */
  ta->type = TA_RANGE;
  ta->range.stride = stride;
  ta->range.sub_ta = NULL;

  /* Link into chain */
  if (!builder->head)
    {
      builder->head = ta;
      builder->tail = ta;
    }
  else
    {
      /* Need to set sub_ta based on previous type */
      if (builder->tail->type == TA_SELECT)
        {
          builder->tail->select.sub_ta = ta;
        }
      else if (builder->tail->type == TA_RANGE)
        {
          builder->tail->range.sub_ta = ta;
        }
      builder->tail = ta;
    }

  return SUCCESS;
}

static err_t
tab_accept_take (
    struct type_accessor_builder *builder,
    error *e)
{
  DBG_ASSERT (type_accessor_builder, builder);

  /* Allocate new accessor */
  struct type_accessor *ta;
  if (builder->head == NULL)
    {
      ta = &builder->ret;
    }
  else
    {
      ta = chunk_malloc (builder->persistent, 1, sizeof *ta, e);
      if (!ta)
        {
          return e->cause_code;
        }
    }

  /* Initialize range accessor */
  ta->type = TA_TAKE;

  /* Link into chain */
  if (!builder->head)
    {
      builder->head = ta;
      builder->tail = ta;
    }
  else
    {
      /* Need to set sub_ta based on previous type */
      if (builder->tail->type == TA_SELECT)
        {
          builder->tail->select.sub_ta = ta;
        }
      else if (builder->tail->type == TA_RANGE)
        {
          builder->tail->range.sub_ta = ta;
        }
      builder->tail = ta;
    }

  return SUCCESS;
}

err_t
tab_build (struct type_accessor *dest, struct type_accessor_builder *builder, error *e)
{
  DBG_ASSERT (type_accessor_builder, builder);

  err_t_wrap (tab_accept_take (builder, e), e);

  *dest = builder->ret;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, type_accessor_builder)
{
  error err = error_create ();

  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);

  /* 0. freshly-created builder must be clean */
  struct type_accessor_builder builder;
  tab_create (&builder, &arena);
  test_fail_if (builder.head != NULL);
  test_fail_if (builder.tail != NULL);

  /* 1. build with empty builder must fail */
  struct type_accessor acc;
  test_assert_int_equal (tab_build (&acc, &builder, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 2. accept a select accessor */
  struct string key1 = strfcstr ("field1");
  test_assert_int_equal (tab_accept_select (&builder, key1, &err), SUCCESS);
  test_fail_if_null (builder.head);
  test_fail_if_null (builder.tail);

  /* 3. accept a range accessor */
  test_assert_int_equal (tab_accept_range (&builder, ustridefrom (0, 10, 2), &err), SUCCESS);

  /* 4. accept another select accessor */
  struct string key2 = strfcstr ("field2");
  test_assert_int_equal (tab_accept_select (&builder, key2, &err), SUCCESS);

  /* 5. successful build */
  test_assert_int_equal (tab_build (&acc, &builder, &err), SUCCESS);

  /* 6. verify chain */
  test_assert_int_equal (acc.type, TA_SELECT);
  test_assert_int_equal (string_equal (acc.select.key, key1), true);
  test_fail_if_null (acc.select.sub_ta);

  struct type_accessor *range_acc = acc.select.sub_ta;
  test_assert_int_equal (range_acc->type, TA_RANGE);
  test_assert_int_equal (range_acc->range.stride.start, 0);
  test_assert_int_equal (range_acc->range.stride.stop, 10);
  test_assert_int_equal (range_acc->range.stride.step, 2);
  test_fail_if_null (range_acc->range.sub_ta);

  struct type_accessor *select_acc = range_acc->range.sub_ta;
  test_assert_int_equal (select_acc->type, TA_SELECT);
  test_assert_int_equal (string_equal (select_acc->select.key, key2), true);
  test_fail_if (select_acc->select.sub_ta != NULL);

  chunk_alloc_free_all (&arena);
}
#endif
