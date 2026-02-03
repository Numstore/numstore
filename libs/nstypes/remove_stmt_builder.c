#include <numstore/types/remove_stmt_builder.h>
#include <numstore/types/statement.h>

#include <numstore/core/assert.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    struct remove_builder, remove_builder, s,
    {
      ASSERT (s);
    })

void
rmb_create (struct remove_builder *dest, struct chunk_alloc *persistent)
{
  *dest = (struct remove_builder){
    .vref = { .vname = { .len = 0, .data = NULL }, .alias = { .len = 0, .data = NULL } },
    .gstride = { .start = 0, .step = 0, .stop = 0, .present = 0 },
    .has_gstride = false,
    .persistent = persistent,
  };

  DBG_ASSERT (remove_builder, dest);
}

err_t
rmb_accept_vref (struct remove_builder *builder, struct vref ref, error *e)
{
  DBG_ASSERT (remove_builder, builder);

  if (ref.vname.len == 0)
    {
      return error_causef (e, ERR_INTERP, "Variable name cannot be empty");
    }

  /* Copy vname to persistent memory */
  ref.vname.data = chunk_alloc_move_mem (builder->persistent, ref.vname.data, ref.vname.len, e);
  if (!ref.vname.data)
    {
      return e->cause_code;
    }

  /* Copy alias if present */
  if (ref.alias.len > 0)
    {
      ref.alias.data = chunk_alloc_move_mem (builder->persistent, ref.alias.data, ref.alias.len, e);
      if (!ref.alias.data)
        {
          return e->cause_code;
        }
    }

  builder->vref = ref;
  return SUCCESS;
}

err_t
rmb_accept_stride (struct remove_builder *builder, struct user_stride stride, error *e)
{
  DBG_ASSERT (remove_builder, builder);

  builder->gstride = stride;
  builder->has_gstride = true;
  return SUCCESS;
}

err_t
rmb_build (struct remove_stmt *dest, struct remove_builder *builder, error *e)
{
  DBG_ASSERT (remove_builder, builder);
  ASSERT (dest);

  if (builder->vref.vname.len == 0)
    {
      return error_causef (e, ERR_INTERP, "vref not set");
    }

  dest->ref = builder->vref;
  dest->gstride = builder->gstride;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, remove_stmt_builder)
{
  error err = error_create ();

  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);

  /* 0. freshly-created builder */
  struct remove_builder builder;
  rmb_create (&builder, &arena);
  test_assert_int_equal (builder.vref.vname.len, 0);
  test_assert_int_equal (builder.has_gstride, false);

  /* 1. build with no vref should fail */
  struct remove_stmt stmt = { 0 };
  test_assert_int_equal (rmb_build (&stmt, &builder, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 2. empty vref should fail */
  struct vref empty_ref = { 0 };
  test_assert_int_equal (rmb_accept_vref (&builder, empty_ref, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 3. accept valid vref */
  struct vref ref = {
    .vname = strfcstr ("my_array"),
    .alias = { 0 },
  };
  test_assert_int_equal (rmb_accept_vref (&builder, ref, &err), SUCCESS);

  /* 4. accept stride */
  struct user_stride stride = { .start = 5, .stop = 15, .step = 2, .present = START_PRESENT | STOP_PRESENT | STEP_PRESENT };
  test_assert_int_equal (rmb_accept_stride (&builder, stride, &err), SUCCESS);
  test_assert_int_equal (builder.has_gstride, true);

  /* 5. successful build */
  test_assert_int_equal (rmb_build (&stmt, &builder, &err), SUCCESS);
  test_assert_int_equal (stmt.ref.vname.len, ref.vname.len);
  test_assert_int_equal (stmt.gstride.start, 5);
  test_assert_int_equal (stmt.gstride.stop, 15);

  chunk_alloc_free_all (&arena);
}
#endif
