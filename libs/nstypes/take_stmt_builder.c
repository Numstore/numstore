#include <numstore/types/take_stmt_builder.h>
#include <numstore/types/statement.h>

#include <numstore/core/assert.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    struct take_builder, take_builder, s,
    {
      ASSERT (s);
    })

void
tkb_create (
    struct take_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent)
{
  *dest = (struct take_builder){
    .vrefs = { .items = NULL, .len = 0 },
    .accs = { .items = NULL, .len = 0 },
    .gstride = { .start = 0, .step = 0, .stop = 0, .present = 0 },
    .has_gstride = false,
  };

  DBG_ASSERT (take_builder, dest);
}

err_t
tkb_accept_vref_list (
    struct take_builder *builder,
    struct vref_list vrefs,
    error *e)
{
  DBG_ASSERT (take_builder, builder);

  builder->vrefs = vrefs;
  return SUCCESS;
}

err_t
tkb_accept_accessor_list (
    struct take_builder *builder,
    struct type_accessor_list *acc,
    error *e)
{
  DBG_ASSERT (take_builder, builder);

  if (acc)
    {
      builder->accs = *acc;
    }
  return SUCCESS;
}

err_t
tkb_accept_stride (
    struct take_builder *builder,
    struct user_stride stride,
    error *e)
{
  DBG_ASSERT (take_builder, builder);

  builder->gstride = stride;
  builder->has_gstride = true;
  return SUCCESS;
}

err_t
tkb_build (struct take_stmt *dest, struct take_builder *builder, error *e)
{
  DBG_ASSERT (take_builder, builder);
  ASSERT (dest);

  if (builder->vrefs.len == 0)
    {
      return error_causef (e, ERR_INTERP, "vref_list is empty");
    }

  dest->vrefs = builder->vrefs;
  dest->acc = builder->accs;
  dest->gstride = builder->gstride;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, take_stmt_builder)
{
  error err = error_create ();

  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);

  /* 0. freshly-created builder */
  struct take_builder builder;
  tkb_create (&builder, &arena, &arena);
  test_assert_int_equal (builder.vrefs.len, 0);
  test_assert_int_equal (builder.has_gstride, false);

  /* 1. build with no vrefs should fail */
  struct take_stmt stmt = { 0 };
  test_assert_int_equal (tkb_build (&stmt, &builder, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 2. create vref_list */
  struct vref_list_builder vrlb;
  vrlb_create (&vrlb, &arena, &arena);
  test_assert_int_equal (vrlb_accept (&vrlb, "data", NULL, &err), SUCCESS);

  struct vref_list vrefs;
  test_assert_int_equal (vrlb_build (&vrefs, &vrlb, &err), SUCCESS);

  /* 3. accept vref_list */
  test_assert_int_equal (tkb_accept_vref_list (&builder, vrefs, &err), SUCCESS);

  /* 4. successful build */
  test_assert_int_equal (tkb_build (&stmt, &builder, &err), SUCCESS);
  test_assert_int_equal (stmt.vrefs.len, 1);

  chunk_alloc_free_all (&arena);
}
#endif
