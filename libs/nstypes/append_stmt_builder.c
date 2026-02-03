#include <numstore/types/append_stmt_builder.h>
#include <numstore/types/statement.h>

#include <numstore/core/assert.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    struct append_builder, append_builder, s,
    {
      ASSERT (s);
    })

void
apb_create (struct append_builder *dest, struct chunk_alloc *persistent)
{
  *dest = (struct append_builder){
    .vname = { .len = 0, .data = NULL },
    .nelems = 0,
    .persistent = persistent,
  };

  DBG_ASSERT (append_builder, dest);
}

err_t
apb_accept_vname (struct append_builder *dest, struct string vname, error *e)
{
  DBG_ASSERT (append_builder, dest);

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
apb_accept_nelems (struct append_builder *dest, b_size nelems, error *e)
{
  DBG_ASSERT (append_builder, dest);

  dest->nelems = nelems;
  return SUCCESS;
}

err_t
apb_build (struct insert_stmt *dest, struct append_builder *builder, error *e)
{
  DBG_ASSERT (append_builder, builder);
  ASSERT (dest);

  if (builder->vname.len == 0)
    {
      return error_causef (e, ERR_INTERP, "Variable name not set");
    }

  dest->vname = builder->vname;
  dest->ofst = 0; /* Append has no offset, uses 0 as placeholder */
  dest->nelems = builder->nelems;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, append_stmt_builder)
{
  error err = error_create ();

  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);

  /* 0. freshly-created builder must be clean */
  struct append_builder builder;
  apb_create (&builder, &arena);
  test_assert_int_equal (builder.vname.len, 0);
  test_assert_int_equal (builder.nelems, 0);

  /* 1. build with no name should fail */
  struct insert_stmt stmt = { 0 };
  test_assert_int_equal (apb_build (&stmt, &builder, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 2. accept valid name */
  struct string name = strfcstr ("my_array");
  test_assert_int_equal (apb_accept_vname (&builder, name, &err), SUCCESS);

  /* 3. accept nelems */
  test_assert_int_equal (apb_accept_nelems (&builder, 3, &err), SUCCESS);

  /* 4. successful build */
  test_assert_int_equal (apb_build (&stmt, &builder, &err), SUCCESS);
  test_assert_int_equal (stmt.vname.len, name.len);
  test_assert_int_equal (stmt.nelems, 3);

  chunk_alloc_free_all (&arena);
}
#endif
