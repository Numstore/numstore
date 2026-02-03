#include <numstore/types/type_accessor_list_builder.h>

#include <numstore/core/assert.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    struct type_accessor_list_builder, type_accessor_list_builder, s,
    {
      ASSERT (s);
    })

void
talb_create (
    struct type_accessor_list_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent)
{
  *dest = (struct type_accessor_list_builder){
    .head = NULL,
    .count = 0,
    .temp = temp,
    .persistent = persistent,
  };

  DBG_ASSERT (type_accessor_list_builder, dest);
}

err_t
talb_accept (
    struct type_accessor_list_builder *builder,
    struct type_accessor *acc,
    error *e)
{
  DBG_ASSERT (type_accessor_list_builder, builder);

  /* Allocate new node */
  struct ta_llnode *node = chunk_malloc (builder->temp, 1, sizeof *node, e);
  if (!node)
    {
      return e->cause_code;
    }

  llnode_init (&node->link);
  node->acc = acc;

  /* Add to list */
  if (!builder->head)
    {
      builder->head = &node->link;
    }
  else
    {
      list_append (&builder->head, &node->link);
    }
  builder->count++;

  return SUCCESS;
}

err_t
talb_build (
    struct type_accessor_list *dest,
    struct type_accessor_list_builder *builder,
    error *e)
{
  DBG_ASSERT (type_accessor_list_builder, builder);
  ASSERT (dest);

  /* Empty list is valid - just return empty */
  if (builder->count == 0)
    {
      dest->items = NULL;
      dest->len = 0;
      return SUCCESS;
    }

  /* Allocate array in persistent memory */
  struct type_accessor *items = chunk_malloc (builder->persistent, builder->count, sizeof *items, e);
  if (!items)
    {
      return e->cause_code;
    }

  /* Copy items from linked list */
  u32 i = 0;
  for (struct llnode *it = builder->head; it; it = it->next)
    {
      struct ta_llnode *node = container_of (it, struct ta_llnode, link);
      items[i++] = *node->acc;
    }

  dest->items = items;
  dest->len = builder->count;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, type_accessor_list_builder)
{
  error err = error_create ();

  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);

  /* 0. freshly-created builder must be clean */
  struct type_accessor_list_builder builder;
  talb_create (&builder, &arena, &arena);
  test_fail_if (builder.head != NULL);
  test_assert_int_equal (builder.count, 0);

  /* 1. build with empty builder is valid - returns empty list */
  struct type_accessor_list list = { 0 };
  test_assert_int_equal (talb_build (&list, &builder, &err), SUCCESS);
  test_assert_int_equal (list.len, 0);

  /* 2. create a type accessor and accept it */
  struct type_accessor_builder tab;
  tab_create (&tab, &arena, &arena);
  struct string key1 = strfcstr ("field1");
  test_assert_int_equal (tab_accept_select (&tab, key1, &err), SUCCESS);

  struct type_accessor *acc1;
  test_assert_int_equal (tab_build (&acc1, &tab, &err), SUCCESS);

  test_assert_int_equal (talb_accept (&builder, acc1, &err), SUCCESS);
  test_assert_int_equal (builder.count, 1);

  /* 3. create another type accessor and accept it */
  struct type_accessor_builder tab2;
  tab_create (&tab2, &arena, &arena);
  test_assert_int_equal (tab_accept_range (&tab2, 0, 5, 1, &err), SUCCESS);

  struct type_accessor *acc2;
  test_assert_int_equal (tab_build (&acc2, &tab2, &err), SUCCESS);

  test_assert_int_equal (talb_accept (&builder, acc2, &err), SUCCESS);
  test_assert_int_equal (builder.count, 2);

  /* 4. successful build */
  test_assert_int_equal (talb_build (&list, &builder, &err), SUCCESS);
  test_assert_int_equal (list.len, 2);
  test_fail_if_null (list.items);

  /* 5. verify contents */
  test_assert_int_equal (list.items[0].type, TA_SELECT);
  test_assert_int_equal (list.items[1].type, TA_RANGE);

  chunk_alloc_free_all (&arena);
}
#endif
