#include <numstore/types/union_builder.h>
#include <numstore/types/union.h>

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
    .persistent = persistent,
  };

  DBG_ASSERT (union_builder, dest);
}

err_t
unb_accept_kvt_list (struct union_builder *builder, struct kvt_list list, error *e)
{
  DBG_ASSERT (union_builder, builder);

  /* Validation - at least one key-value pair required */
  if (list.len == 0)
    {
      return error_causef (e, ERR_INTERP, "kvt_list must have at least one entry");
    }

  return SUCCESS;
}

err_t
unb_build (struct union_t *dest, struct union_builder *builder, struct kvt_list list, error *e)
{
  DBG_ASSERT (union_builder, builder);
  ASSERT (dest);

  /* Validate list */
  err_t_wrap (unb_accept_kvt_list (builder, list, e), e);

  dest->keys = list.keys;
  dest->types = list.types;
  dest->len = list.len;

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

  struct string key1 = strfcstr ("variant1");
  struct string key2 = strfcstr ("variant2");
  struct type t_u32 = (struct type){ .type = T_PRIM, .p = U32 };
  struct type t_f64 = (struct type){ .type = T_PRIM, .p = F64 };

  test_assert_int_equal (kvlb_accept_key (&kvlb, key1, &err), SUCCESS);
  test_assert_int_equal (kvlb_accept_type (&kvlb, t_u32, &err), SUCCESS);
  test_assert_int_equal (kvlb_accept_key (&kvlb, key2, &err), SUCCESS);
  test_assert_int_equal (kvlb_accept_type (&kvlb, t_f64, &err), SUCCESS);

  struct kvt_list list;
  test_assert_int_equal (kvlb_build (&list, &kvlb, &err), SUCCESS);

  /* Now use union_builder */
  struct union_builder ub;
  unb_create (&ub, &arena);

  /* Build union from kvt_list */
  struct union_t un = { 0 };
  test_assert_int_equal (unb_build (&un, &ub, list, &err), SUCCESS);
  test_assert_int_equal (un.len, 2);
  test_fail_if_null (un.keys);
  test_fail_if_null (un.types);

  /* Verify contents */
  test_assert_int_equal (string_equal (un.keys[0], key1), true);
  test_assert_int_equal (string_equal (un.keys[1], key2), true);
  test_assert_int_equal (un.types[0].p, U32);
  test_assert_int_equal (un.types[1].p, F64);

  /* Empty list should fail */
  struct kvt_list empty_list = { .len = 0, .keys = NULL, .types = NULL };
  struct union_t un_fail = { 0 };
  test_assert_int_equal (unb_build (&un_fail, &ub, empty_list, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  chunk_alloc_free_all (&arena);
}
#endif
