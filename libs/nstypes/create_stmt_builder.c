#include <numstore/types/create_stmt_builder.h>
#include <numstore/types/statement.h>

#include <numstore/core/assert.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    struct create_builder, create_builder, s,
    {
      ASSERT (s);
    })

void
crb_create (struct create_builder *dest, struct chunk_alloc *persistent)
{
  *dest = (struct create_builder){
    .vname = { .len = 0, .data = NULL },
    .vtype = { .type = 0 },
    .persistent = persistent,
  };

  DBG_ASSERT (create_builder, dest);
}

err_t
crb_accept_vname (struct create_builder *dest, struct string vname, error *e)
{
  DBG_ASSERT (create_builder, dest);

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
crb_accept_type (struct create_builder *dest, struct type t, error *e)
{
  DBG_ASSERT (create_builder, dest);

  dest->vtype = t;
  return SUCCESS;
}

err_t
crb_build (struct create_stmt *dest, struct create_builder *builder, error *e)
{
  DBG_ASSERT (create_builder, builder);
  ASSERT (dest);

  if (builder->vname.len == 0)
    {
      return error_causef (e, ERR_INTERP, "Variable name not set");
    }

  dest->vname = builder->vname;
  dest->vtype = builder->vtype;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, create_stmt_builder)
{
  error err = error_create ();

  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);

  /* 0. freshly-created builder must be clean */
  struct create_builder builder;
  crb_create (&builder, &arena);
  test_assert_int_equal (builder.vname.len, 0);

  /* 1. build with no name should fail */
  struct create_stmt stmt = { 0 };
  test_assert_int_equal (crb_build (&stmt, &builder, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 2. empty name should fail */
  struct string empty = { 0 };
  test_assert_int_equal (crb_accept_vname (&builder, empty, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 3. accept valid name */
  struct string name = strfcstr ("my_variable");
  test_assert_int_equal (crb_accept_vname (&builder, name, &err), SUCCESS);

  /* 4. accept type */
  struct type t = (struct type){ .type = T_PRIM, .p = U32 };
  test_assert_int_equal (crb_accept_type (&builder, t, &err), SUCCESS);

  /* 5. successful build */
  test_assert_int_equal (crb_build (&stmt, &builder, &err), SUCCESS);
  test_assert_int_equal (stmt.vname.len, name.len);
  test_assert_int_equal (stmt.vtype.type, T_PRIM);
  test_assert_int_equal (stmt.vtype.p, U32);

  chunk_alloc_free_all (&arena);
}
#endif
