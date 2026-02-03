#include <numstore/types/type_accessor_builder.h>

#include <numstore/core/assert.h>
#include <numstore/test/testing.h>
#include <numstore/types/type_accessor.h>

DEFINE_DBG_ASSERT (
    struct type_accessor_builder, type_accessor_builder, s,
    {
      ASSERT (s);
    })

void
tab_create (
    struct type_accessor_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent)
{
  *dest = (struct type_accessor_builder){
    .head = NULL,
    .tail = NULL,
    .temp = temp,
    .persistent = persistent,
  };

  DBG_ASSERT (type_accessor_builder, dest);
}

err_t
tab_accept_select (struct type_accessor_builder *builder, struct string key, error *e)
{
  DBG_ASSERT (type_accessor_builder, builder);

  /* Allocate new accessor */
  struct type_accessor *ta = chunk_malloc (builder->temp, 1, sizeof *ta, e);
  if (!ta)
    {
      return e->cause_code;
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
    t_size start,
    t_size stop,
    t_size step,
    error *e)
{
  DBG_ASSERT (type_accessor_builder, builder);

  /* Allocate new accessor */
  struct type_accessor *ta = chunk_malloc (builder->temp, 1, sizeof *ta, e);
  if (!ta)
    {
      return e->cause_code;
    }

  /* Initialize range accessor */
  ta->type = TA_RANGE;
  ta->range.start = start;
  ta->range.stop = stop;
  ta->range.step = step;
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

err_t
tab_build (struct type_accessor **dest, struct type_accessor_builder *builder, error *e)
{
  DBG_ASSERT (type_accessor_builder, builder);

  if (!builder->head)
    {
      return error_causef (e, ERR_INTERP, "Type accessor builder is empty");
    }

  /* Move the chain from temp to persistent memory */
  struct type_accessor *current = builder->head;
  struct type_accessor *prev_persistent = NULL;
  struct type_accessor *head_persistent = NULL;

  while (current)
    {
      /* Allocate in persistent memory */
      struct type_accessor *persistent_node = chunk_malloc (builder->persistent, 1, sizeof *persistent_node, e);
      if (!persistent_node)
        {
          return e->cause_code;
        }

      /* Copy the node */
      *persistent_node = *current;

      /* Link into persistent chain */
      if (!head_persistent)
        {
          head_persistent = persistent_node;
        }
      if (prev_persistent)
        {
          if (prev_persistent->type == TA_SELECT)
            {
              prev_persistent->select.sub_ta = persistent_node;
            }
          else if (prev_persistent->type == TA_RANGE)
            {
              prev_persistent->range.sub_ta = persistent_node;
            }
        }

      /* Advance */
      prev_persistent = persistent_node;
      if (current->type == TA_SELECT)
        {
          current = current->select.sub_ta;
        }
      else if (current->type == TA_RANGE)
        {
          current = current->range.sub_ta;
        }
      else
        {
          current = NULL;
        }
    }

  /* Clear the final node's sub_ta */
  if (prev_persistent)
    {
      if (prev_persistent->type == TA_SELECT)
        {
          prev_persistent->select.sub_ta = NULL;
        }
      else if (prev_persistent->type == TA_RANGE)
        {
          prev_persistent->range.sub_ta = NULL;
        }
    }

  *dest = head_persistent;
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
  tab_create (&builder, &arena, &arena);
  test_fail_if (builder.head != NULL);
  test_fail_if (builder.tail != NULL);

  /* 1. build with empty builder must fail */
  struct type_accessor *acc = NULL;
  test_assert_int_equal (tab_build (&acc, &builder, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 2. accept a select accessor */
  struct string key1 = strfcstr ("field1");
  test_assert_int_equal (tab_accept_select (&builder, key1, &err), SUCCESS);
  test_fail_if_null (builder.head);
  test_fail_if_null (builder.tail);

  /* 3. accept a range accessor */
  test_assert_int_equal (tab_accept_range (&builder, 0, 10, 2, &err), SUCCESS);

  /* 4. accept another select accessor */
  struct string key2 = strfcstr ("field2");
  test_assert_int_equal (tab_accept_select (&builder, key2, &err), SUCCESS);

  /* 5. successful build */
  test_assert_int_equal (tab_build (&acc, &builder, &err), SUCCESS);
  test_fail_if_null (acc);

  /* 6. verify chain */
  test_assert_int_equal (acc->type, TA_SELECT);
  test_assert_int_equal (string_equal (acc->select.key, key1), true);
  test_fail_if_null (acc->select.sub_ta);

  struct type_accessor *range_acc = acc->select.sub_ta;
  test_assert_int_equal (range_acc->type, TA_RANGE);
  test_assert_int_equal (range_acc->range.start, 0);
  test_assert_int_equal (range_acc->range.stop, 10);
  test_assert_int_equal (range_acc->range.step, 2);
  test_fail_if_null (range_acc->range.sub_ta);

  struct type_accessor *select_acc = range_acc->range.sub_ta;
  test_assert_int_equal (select_acc->type, TA_SELECT);
  test_assert_int_equal (string_equal (select_acc->select.key, key2), true);
  test_fail_if (select_acc->select.sub_ta != NULL);

  chunk_alloc_free_all (&arena);
}
#endif
