#include <numstore/types/vref_list_builder.h>

#include <numstore/core/assert.h>
#include <numstore/intf/stdlib.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    struct vref_list_builder, vref_list_builder, s,
    {
      ASSERT (s);
    })

void
vrlb_create (
    struct vref_list_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent)
{
  *dest = (struct vref_list_builder){
    .head = NULL,
    .count = 0,
    .temp = temp,
    .persistent = persistent,
  };

  DBG_ASSERT (vref_list_builder, dest);
}

err_t
vrlb_accept (
    struct vref_list_builder *builder,
    const char *name,
    const char *ref,
    error *e)
{
  DBG_ASSERT (vref_list_builder, builder);

  /* Allocate new node */
  struct vref_llnode *node = chunk_malloc (builder->temp, 1, sizeof *node, e);
  if (!node)
    {
      return e->cause_code;
    }

  llnode_init (&node->link);

  /* Copy name to persistent memory */
  u32 name_len = (u32)i_strlen (name);
  const char *name_data = chunk_alloc_move_mem (builder->persistent, name, name_len, e);
  if (!name_data)
    {
      return e->cause_code;
    }
  node->ref.vname.len = name_len;
  node->ref.vname.data = name_data;

  /* Copy ref/alias to persistent memory (can be empty) */
  if (ref && ref[0] != '\0')
    {
      u32 ref_len = (u32)i_strlen (ref);
      const char *ref_data = chunk_alloc_move_mem (builder->persistent, ref, ref_len, e);
      if (!ref_data)
        {
          return e->cause_code;
        }
      node->ref.alias.len = ref_len;
      node->ref.alias.data = ref_data;
    }
  else
    {
      node->ref.alias.len = 0;
      node->ref.alias.data = NULL;
    }

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
vrlb_build (
    struct vref_list *dest,
    struct vref_list_builder *builder,
    error *e)
{
  DBG_ASSERT (vref_list_builder, builder);
  ASSERT (dest);

  if (builder->count == 0)
    {
      return error_causef (e, ERR_INTERP, "vref_list_builder is empty");
    }

  /* Allocate array in persistent memory */
  struct vref *items = chunk_malloc (builder->persistent, builder->count, sizeof *items, e);
  if (!items)
    {
      return e->cause_code;
    }

  /* Copy items from linked list */
  u32 i = 0;
  for (struct llnode *it = builder->head; it; it = it->next)
    {
      struct vref_llnode *node = container_of (it, struct vref_llnode, link);
      items[i++] = node->ref;
    }

  dest->items = items;
  dest->len = builder->count;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, vref_list_builder)
{
  error err = error_create ();

  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);

  /* 0. freshly-created builder must be clean */
  struct vref_list_builder builder;
  vrlb_create (&builder, &arena, &arena);
  test_fail_if (builder.head != NULL);
  test_assert_int_equal (builder.count, 0);

  /* 1. build with empty builder must fail */
  struct vref_list list = { 0 };
  test_assert_int_equal (vrlb_build (&list, &builder, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 2. accept first vref with name only */
  test_assert_int_equal (vrlb_accept (&builder, "var1", NULL, &err), SUCCESS);
  test_assert_int_equal (builder.count, 1);

  /* 3. accept second vref with name and alias */
  test_assert_int_equal (vrlb_accept (&builder, "var2", "alias2", &err), SUCCESS);
  test_assert_int_equal (builder.count, 2);

  /* 4. accept third vref with empty alias */
  test_assert_int_equal (vrlb_accept (&builder, "var3", "", &err), SUCCESS);
  test_assert_int_equal (builder.count, 3);

  /* 5. successful build */
  test_assert_int_equal (vrlb_build (&list, &builder, &err), SUCCESS);
  test_assert_int_equal (list.len, 3);
  test_fail_if_null (list.items);

  /* 6. verify contents */
  test_assert_int_equal (list.items[0].vname.len, 4);
  test_assert_int_equal (i_memcmp (list.items[0].vname.data, "var1", 4), 0);
  test_fail_if (list.items[0].alias.data != NULL);

  test_assert_int_equal (list.items[1].vname.len, 4);
  test_assert_int_equal (i_memcmp (list.items[1].vname.data, "var2", 4), 0);
  test_assert_int_equal (list.items[1].alias.len, 6);
  test_assert_int_equal (i_memcmp (list.items[1].alias.data, "alias2", 6), 0);

  test_assert_int_equal (list.items[2].vname.len, 4);
  test_assert_int_equal (i_memcmp (list.items[2].vname.data, "var3", 4), 0);
  test_fail_if (list.items[2].alias.data != NULL);

  chunk_alloc_free_all (&arena);
}
#endif
