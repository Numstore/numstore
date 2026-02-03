#include <numstore/types/read_stmt_builder.h>
#include <numstore/types/statement.h>

#include <numstore/core/assert.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    struct read_builder, read_builder, s,
    {
      ASSERT (s);
    })

void
rdb_create (
    struct read_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent)
{
  *dest = (struct read_builder){
    .vrefs = { .items = NULL, .len = 0 },
    .acc = { .items = NULL, .len = 0 },
    .gstride = { .start = 0, .step = 0, .stop = 0, .present = 0 },
    .has_gstride = false,
  };

  DBG_ASSERT (read_builder, dest);
}

err_t
rdb_accept_vref_list (
    struct read_builder *builder,
    struct vref_list list,
    error *e)
{
  DBG_ASSERT (read_builder, builder);

  builder->vrefs = list;
  return SUCCESS;
}

err_t
rdb_accept_accessor_list (
    struct read_builder *builder,
    struct type_accessor_list *acc,
    error *e)
{
  DBG_ASSERT (read_builder, builder);

  if (acc)
    {
      builder->acc = *acc;
    }
  return SUCCESS;
}

err_t
rdb_accept_stride (
    struct read_builder *builder,
    struct user_stride stride,
    error *e)
{
  DBG_ASSERT (read_builder, builder);

  builder->gstride = stride;
  builder->has_gstride = true;
  return SUCCESS;
}

err_t
rdb_build (struct read_stmt *dest, struct read_builder *builder, error *e)
{
  DBG_ASSERT (read_builder, builder);
  ASSERT (dest);

  if (builder->vrefs.len == 0)
    {
      return error_causef (e, ERR_INTERP, "vref_list is empty");
    }

  dest->vrefs = builder->vrefs;
  dest->acc = builder->acc;
  dest->gstride = builder->gstride;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, read_stmt_builder)
{
  error err = error_create ();

  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);

  /* 0. freshly-created builder */
  struct read_builder builder;
  rdb_create (&builder, &arena, &arena);
  test_assert_int_equal (builder.vrefs.len, 0);
  test_assert_int_equal (builder.has_gstride, false);

  /* 1. build with no vrefs should fail */
  struct read_stmt stmt = { 0 };
  test_assert_int_equal (rdb_build (&stmt, &builder, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 2. create vref_list */
  struct vref_list_builder vrlb;
  vrlb_create (&vrlb, &arena, &arena);
  test_assert_int_equal (vrlb_accept (&vrlb, "var1", "alias1", &err), SUCCESS);
  test_assert_int_equal (vrlb_accept (&vrlb, "var2", NULL, &err), SUCCESS);

  struct vref_list vrefs;
  test_assert_int_equal (vrlb_build (&vrefs, &vrlb, &err), SUCCESS);

  /* 3. accept vref_list */
  test_assert_int_equal (rdb_accept_vref_list (&builder, vrefs, &err), SUCCESS);

  /* 4. accept stride */
  struct user_stride stride = { .start = 0, .stop = 10, .step = 1, .present = STOP_PRESENT | STEP_PRESENT };
  test_assert_int_equal (rdb_accept_stride (&builder, stride, &err), SUCCESS);
  test_assert_int_equal (builder.has_gstride, true);

  /* 5. successful build */
  test_assert_int_equal (rdb_build (&stmt, &builder, &err), SUCCESS);
  test_assert_int_equal (stmt.vrefs.len, 2);
  test_assert_int_equal (stmt.gstride.stop, 10);

  chunk_alloc_free_all (&arena);
}
#endif
