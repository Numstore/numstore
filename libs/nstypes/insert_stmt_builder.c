#include <numstore/types/insert_stmt_builder.h>
#include <numstore/types/statement.h>

#include <numstore/core/assert.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    struct insert_builder, insert_builder, s,
    {
      ASSERT (s);
    })

void
inb_create (struct insert_builder *dest, struct chunk_alloc *persistent)
{
  *dest = (struct insert_builder){
    .vname = { .len = 0, .data = NULL },
    .ofst = 0,
    .nelems = 0,
    .persistent = persistent,
  };

  DBG_ASSERT (insert_builder, dest);
}

err_t
inb_accept_vname (struct insert_builder *dest, struct string vname, error *e)
{
  DBG_ASSERT (insert_builder, dest);

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
inb_accept_ofst (struct insert_builder *dest, b_size ofst, error *e)
{
  DBG_ASSERT (insert_builder, dest);

  dest->ofst = ofst;
  return SUCCESS;
}

err_t
inb_accept_nelems (struct insert_builder *dest, b_size nelems, error *e)
{
  DBG_ASSERT (insert_builder, dest);

  dest->nelems = nelems;
  return SUCCESS;
}

err_t
inb_build (struct insert_stmt *dest, struct insert_builder *builder, error *e)
{
  DBG_ASSERT (insert_builder, builder);
  ASSERT (dest);

  if (builder->vname.len == 0)
    {
      return error_causef (e, ERR_INTERP, "Variable name not set");
    }

  dest->vname = builder->vname;
  dest->ofst = builder->ofst;
  dest->nelems = builder->nelems;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, insert_stmt_builder)
{
  error err = error_create ();

  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);

  /* 0. freshly-created builder must be clean */
  struct insert_builder builder;
  inb_create (&builder, &arena);
  test_assert_int_equal (builder.vname.len, 0);
  test_assert_int_equal (builder.ofst, 0);
  test_assert_int_equal (builder.nelems, 0);

  /* 1. build with no name should fail */
  struct insert_stmt stmt = { 0 };
  test_assert_int_equal (inb_build (&stmt, &builder, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 2. accept valid name */
  struct string name = strfcstr ("my_array");
  test_assert_int_equal (inb_accept_vname (&builder, name, &err), SUCCESS);

  /* 3. accept offset */
  test_assert_int_equal (inb_accept_ofst (&builder, 10, &err), SUCCESS);

  /* 4. accept nelems */
  test_assert_int_equal (inb_accept_nelems (&builder, 5, &err), SUCCESS);

  /* 5. successful build */
  test_assert_int_equal (inb_build (&stmt, &builder, &err), SUCCESS);
  test_assert_int_equal (stmt.vname.len, name.len);
  test_assert_int_equal (stmt.ofst, 10);
  test_assert_int_equal (stmt.nelems, 5);

  chunk_alloc_free_all (&arena);
}
#endif
