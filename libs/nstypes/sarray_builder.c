#include <numstore/types/sarray_builder.h>
#include <numstore/types/sarray.h>
#include <numstore/types/types.h>

#include <numstore/core/assert.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    struct sarray_builder, sarray_builder, s,
    {
      ASSERT (s);
    })

void
sab_create (struct sarray_builder *dest, struct chunk_alloc *temp, struct chunk_alloc *persistent)
{
  *dest = (struct sarray_builder){
    .head = NULL,
    .type = NULL,
    .temp = temp,
    .persistent = persistent,
  };

  DBG_ASSERT (sarray_builder, dest);
}

err_t
sab_accept_dim (struct sarray_builder *eb, u32 dim, error *e)
{
  DBG_ASSERT (sarray_builder, eb);

  u16 idx = (u16)list_length (eb->head);
  struct llnode *slot = llnode_get_n (eb->head, idx);
  struct dim_llnode *node;

  if (slot)
    {
      node = container_of (slot, struct dim_llnode, link);
    }
  else
    {
      node = chunk_malloc (eb->temp, 1, sizeof *node, e);
      if (!node)
        {
          return e->cause_code;
        }
      llnode_init (&node->link);
      if (!eb->head)
        {
          eb->head = &node->link;
        }
      else
        {
          list_append (&eb->head, &node->link);
        }
    }

  node->dim = dim;
  return SUCCESS;
}

err_t
sab_accept_type (struct sarray_builder *eb, struct type t, error *e)
{
  DBG_ASSERT (sarray_builder, eb);

  if (eb->type)
    {
      return error_causef (
          e, ERR_INTERP,
          "type already set");
    }

  struct type *tp = chunk_malloc (eb->temp, 1, sizeof *tp, e);
  if (!tp)
    {
      return e->cause_code;
    }

  *tp = t;
  eb->type = tp;
  return SUCCESS;
}

err_t
sab_build (struct sarray_t *persistent, struct sarray_builder *eb, error *e)
{
  DBG_ASSERT (sarray_builder, eb);
  ASSERT (persistent);

  if (!eb->type)
    {
      return error_causef (
          e, ERR_INTERP,
          "type not set");
    }

  u16 rank = (u16)list_length (eb->head);
  if (rank == 0)
    {
      return error_causef (
          e, ERR_INTERP,
          "no dims to build");
    }

  u32 *dims = chunk_malloc (eb->persistent, rank, sizeof *dims, e);
  if (!dims)
    {
      return e->cause_code;
    }

  /* Copy type to persistent memory (eb->type is on temp) */
  struct type *t = chunk_malloc (eb->persistent, 1, sizeof *t, e);
  if (!t)
    {
      return e->cause_code;
    }
  *t = *eb->type;

  u16 i = 0;
  for (struct llnode *it = eb->head; it; it = it->next)
    {
      struct dim_llnode *dn = container_of (it, struct dim_llnode, link);
      dims[i++] = dn->dim;
    }

  persistent->rank = rank;
  persistent->dims = dims;
  persistent->t = t;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, sarray_builder)
{
  error err = error_create ();

  /* provide two fixed-size allocators for nodes + dims array */
  struct chunk_alloc persistent;
  chunk_alloc_create_default (&persistent);

  /* 0. freshly-created builder must be clean */
  struct sarray_builder sb;
  sab_create (&sb, &persistent, &persistent);
  test_fail_if (sb.head != NULL);
  test_fail_if (sb.type != NULL);

  /* 1. build without type -> ERR_INTERP */
  struct sarray_t sar = { 0 };
  test_assert_int_equal (sab_build (&sar, &sb, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 2. set type but no dims -> still ERR_INTERP */
  struct type t_u32 = (struct type){ .type = T_PRIM, .p = U32 };
  test_assert_int_equal (sab_accept_type (&sb, t_u32, &err), SUCCESS);
  test_assert_int_equal (sab_build (&sar, &sb, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 3. duplicate type must fail */
  test_assert_int_equal (sab_accept_type (&sb, t_u32, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 4. accept first dim 10 */
  test_assert_int_equal (sab_accept_dim (&sb, 10, &err), SUCCESS);

  /* 5. successful build now that we have type and one dim */
  test_assert_int_equal (sab_build (&sar, &sb, &err), SUCCESS);
  test_assert_int_equal (sar.rank, 1);
  test_fail_if_null (sar.dims);
  test_fail_if_null (sar.t);
  test_assert_int_equal (*sar.dims, 10);
  test_assert_int_equal (sar.t->p, t_u32.p);

  /* 6. accept additional dims and rebuild (rank 3) */
  test_assert_int_equal (sab_accept_dim (&sb, 4, &err), SUCCESS);
  test_assert_int_equal (sab_accept_dim (&sb, 2, &err), SUCCESS);
  test_assert_int_equal (sab_build (&sar, &sb, &err), SUCCESS);
  test_assert_int_equal (sar.rank, 3);
  test_assert_int_equal (sar.dims[0], 10);
  test_assert_int_equal (sar.dims[1], 4);
  test_assert_int_equal (sar.dims[2], 2);

  chunk_alloc_free_all (&persistent);
}
#endif
