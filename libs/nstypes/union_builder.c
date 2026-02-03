#include <numstore/types/struct.h>
#include <numstore/types/union_builder.h>

#include <numstore/core/assert.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    struct union_builder, union_builder, s,
    {
      ASSERT (s);
    })

void
unb_create (struct union_builder *dest, struct chunk_alloc *persistent)
{
  *dest = (struct union_builder){
    .has_list = false,
  };

  DBG_ASSERT (union_builder, dest);
}

err_t
unb_accept_kvt_list (struct union_builder *builder, struct kvt_list list, error *e)
{
  DBG_ASSERT (union_builder, builder);

  if (list.len == 0)
    {
      return error_causef (e, ERR_INTERP, "kvt_list must have at least one entry");
    }

  builder->list = list;
  builder->has_list = true;

  return SUCCESS;
}

err_t
unb_build (struct union_t *dest, struct union_builder *builder, error *e)
{
  DBG_ASSERT (union_builder, builder);
  ASSERT (dest);

  if (!builder->has_list)
    {
      return error_causef (e, ERR_INTERP, "Struct must have a kvt list");
    }

  dest->keys = builder->list.keys;
  dest->types = builder->list.types;
  dest->len = builder->list.len;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, union_builder)
{
  error err = error_create ();

  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);

  /* Create kvt_list using kvt_list_builder */
  struct kvt_list_builder kvlb;
  kvlb_create (&kvlb, &arena, &arena);

  struct string key1 = strfcstr ("field1");
  struct string key2 = strfcstr ("field2");
  struct type t_u32 = (struct type){ .type = T_PRIM, .p = U32 };
  struct type t_f32 = (struct type){ .type = T_PRIM, .p = F32 };

  test_assert_int_equal (kvlb_accept_key (&kvlb, key1, &err), SUCCESS);
  test_assert_int_equal (kvlb_accept_type (&kvlb, t_u32, &err), SUCCESS);
  test_assert_int_equal (kvlb_accept_key (&kvlb, key2, &err), SUCCESS);
  test_assert_int_equal (kvlb_accept_type (&kvlb, t_f32, &err), SUCCESS);

  struct kvt_list list;
  test_assert_int_equal (kvlb_build (&list, &kvlb, &err), SUCCESS);

  /* Now use union_builder */
  struct union_builder sb;
  unb_create (&sb, &arena);

  /* Build struct from kvt_list */
  struct union_t st = { 0 };
  test_assert_int_equal (unb_accept_kvt_list (&sb, list, &err), SUCCESS);
  test_assert_int_equal (unb_build (&st, &sb, &err), SUCCESS);
  test_assert_int_equal (st.len, 2);
  test_fail_if_null (st.keys);
  test_fail_if_null (st.types);

  /* Verify contents */
  test_assert_int_equal (string_equal (st.keys[0], key1), true);
  test_assert_int_equal (string_equal (st.keys[1], key2), true);
  test_assert_int_equal (st.types[0].p, U32);
  test_assert_int_equal (st.types[1].p, F32);

  /* Empty list should fail */
  struct kvt_list empty_list = { .len = 0, .keys = NULL, .types = NULL };
  struct union_t st_fail = { 0 };
  test_assert_int_equal (unb_accept_kvt_list (&sb, empty_list, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  chunk_alloc_free_all (&arena);
}
#endif
