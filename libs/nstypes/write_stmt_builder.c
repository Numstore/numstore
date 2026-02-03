#include <numstore/types/write_stmt_builder.h>
#include <numstore/types/statement.h>

#include <numstore/core/assert.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    struct write_builder, write_builder, s,
    {
      ASSERT (s);
    })

void
wrb_create (
    struct write_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent)
{
  *dest = (struct write_builder){
    .vref = { .vname = { .len = 0, .data = NULL }, .alias = { .len = 0, .data = NULL } },
    .acc = { .items = NULL, .len = 0 },
    .gstride = { .start = 0, .step = 0, .stop = 0, .present = 0 },
    .has_gstride = false,
    .persistent = persistent,
  };

  DBG_ASSERT (write_builder, dest);
}

err_t
wrb_accept_vref (
    struct write_builder *builder,
    struct string name,
    struct string ref,
    error *e)
{
  DBG_ASSERT (write_builder, builder);

  if (name.len == 0)
    {
      return error_causef (e, ERR_INTERP, "Variable name cannot be empty");
    }

  /* Copy name to persistent memory */
  name.data = chunk_alloc_move_mem (builder->persistent, name.data, name.len, e);
  if (!name.data)
    {
      return e->cause_code;
    }

  builder->vref.vname = name;

  /* Copy ref/alias if present */
  if (ref.len > 0)
    {
      ref.data = chunk_alloc_move_mem (builder->persistent, ref.data, ref.len, e);
      if (!ref.data)
        {
          return e->cause_code;
        }
      builder->vref.alias = ref;
    }
  else
    {
      builder->vref.alias = (struct string){ 0 };
    }

  return SUCCESS;
}

err_t
wrb_accept_accessor_list (
    struct write_builder *builder,
    struct type_accessor_list *acc,
    error *e)
{
  DBG_ASSERT (write_builder, builder);

  if (acc)
    {
      builder->acc = *acc;
    }
  return SUCCESS;
}

err_t
wrb_accept_stride (
    struct write_builder *builder,
    struct user_stride stride,
    error *e)
{
  DBG_ASSERT (write_builder, builder);

  builder->gstride = stride;
  builder->has_gstride = true;
  return SUCCESS;
}

err_t
wrb_build (struct write_stmt *dest, struct write_builder *builder, error *e)
{
  DBG_ASSERT (write_builder, builder);
  ASSERT (dest);

  if (builder->vref.vname.len == 0)
    {
      return error_causef (e, ERR_INTERP, "vref not set");
    }

  dest->vref = builder->vref;
  dest->acc = builder->acc;
  dest->gstride = builder->gstride;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, write_stmt_builder)
{
  error err = error_create ();

  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);

  /* 0. freshly-created builder */
  struct write_builder builder;
  wrb_create (&builder, &arena, &arena);
  test_assert_int_equal (builder.vref.vname.len, 0);
  test_assert_int_equal (builder.has_gstride, false);

  /* 1. build with no vref should fail */
  struct write_stmt stmt = { 0 };
  test_assert_int_equal (wrb_build (&stmt, &builder, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 2. empty name should fail */
  struct string empty = { 0 };
  test_assert_int_equal (wrb_accept_vref (&builder, empty, empty, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 3. accept vref */
  struct string name = strfcstr ("target_var");
  struct string ref = strfcstr ("alias");
  test_assert_int_equal (wrb_accept_vref (&builder, name, ref, &err), SUCCESS);

  /* 4. successful build */
  test_assert_int_equal (wrb_build (&stmt, &builder, &err), SUCCESS);
  test_assert_int_equal (stmt.vref.vname.len, name.len);
  test_assert_int_equal (stmt.vref.alias.len, ref.len);

  chunk_alloc_free_all (&arena);
}
#endif
