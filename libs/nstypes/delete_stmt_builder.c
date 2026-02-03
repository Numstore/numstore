#include <numstore/types/delete_stmt_builder.h>
#include <numstore/types/statement.h>

#include <numstore/core/assert.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    struct delete_builder, delete_builder, s,
    {
      ASSERT (s);
    })

void
dlb_create (struct delete_builder *dest, struct chunk_alloc *persistent)
{
  *dest = (struct delete_builder){
    .vname = { .len = 0, .data = NULL },
    .persistent = persistent,
  };

  DBG_ASSERT (delete_builder, dest);
}

err_t
dlb_accept_vname (struct delete_builder *dest, struct string vname, error *e)
{
  DBG_ASSERT (delete_builder, dest);

  if (vname.len == 0)
    {
      return error_causef (e, ERR_INTERP, "Variable name cannot be empty");
    }

  /* Copy to persistent memory */
  vname.data = chunk_alloc_move_mem (dest->persistent, vname.data, vname.len, e);
  if (!vname.data)
    {
      return e->cause_code;
    }

  dest->vname = vname;
  return SUCCESS;
}

err_t
dlb_build (struct delete_stmt *dest, struct delete_builder *builder, error *e)
{
  DBG_ASSERT (delete_builder, builder);
  ASSERT (dest);

  if (builder->vname.len == 0)
    {
      return error_causef (e, ERR_INTERP, "Variable name not set");
    }

  dest->vname = builder->vname;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, delete_stmt_builder)
{
  error err = error_create ();

  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);

  /* 0. freshly-created builder must be clean */
  struct delete_builder builder;
  dlb_create (&builder, &arena);
  test_assert_int_equal (builder.vname.len, 0);

  /* 1. build with no name should fail */
  struct delete_stmt stmt = { 0 };
  test_assert_int_equal (dlb_build (&stmt, &builder, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 2. empty name should fail */
  struct string empty = { 0 };
  test_assert_int_equal (dlb_accept_vname (&builder, empty, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 3. accept valid name */
  struct string name = strfcstr ("my_variable");
  test_assert_int_equal (dlb_accept_vname (&builder, name, &err), SUCCESS);

  /* 4. successful build */
  test_assert_int_equal (dlb_build (&stmt, &builder, &err), SUCCESS);
  test_assert_int_equal (stmt.vname.len, name.len);

  chunk_alloc_free_all (&arena);
}
#endif
