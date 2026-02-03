#include <numstore/types/struct_builder.h>
#include <numstore/types/struct.h>

#include <numstore/core/assert.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    struct struct_builder, struct_builder, s,
    {
      ASSERT (s);
    })

void
stb_create (struct struct_builder *dest, struct chunk_alloc *persistent)
{
  *dest = (struct struct_builder){
    .persistent = persistent,
  };

  DBG_ASSERT (struct_builder, dest);
}

err_t
stb_accept_kvt_list (struct struct_builder *builder, struct kvt_list list, error *e)
{
  DBG_ASSERT (struct_builder, builder);

  /* Validation - at least one key-value pair required */
  if (list.len == 0)
    {
      return error_causef (e, ERR_INTERP, "kvt_list must have at least one entry");
    }

  return SUCCESS;
}

err_t
stb_build (struct struct_t *dest, struct struct_builder *builder, struct kvt_list list, error *e)
{
  DBG_ASSERT (struct_builder, builder);
  ASSERT (dest);

  /* Validate list */
  err_t_wrap (stb_accept_kvt_list (builder, list, e), e);

  dest->keys = list.keys;
  dest->types = list.types;
  dest->len = list.len;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, struct_builder)
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

  /* Now use struct_builder */
  struct struct_builder sb;
  stb_create (&sb, &arena);

  /* Build struct from kvt_list */
  struct struct_t st = { 0 };
  test_assert_int_equal (stb_build (&st, &sb, list, &err), SUCCESS);
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
  struct struct_t st_fail = { 0 };
  test_assert_int_equal (stb_build (&st_fail, &sb, empty_list, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  chunk_alloc_free_all (&arena);
}
#endif
