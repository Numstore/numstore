#include <numstore/core/cbuffer.h>
#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/slab_alloc.h>
#include <numstore/nsdb/nsdb.h>
#include <numstore/nsdb/var_bank.h>
#include <numstore/rptree/rptree_cursor.h>
#include <numstore/var/var_cursor.h>

err_t
var_bank_open (
    struct nsdb *n,
    struct var_bank *dest,
    struct string vname,
    struct user_stride stride,
    error *e)
{
  struct var_cursor *vc = slab_alloc_alloc (&n->slaba, e);
  if (vc == NULL)
    {
      return e->cause_code;
    }

  if (varc_initialize (vc, n->p, e))
    {
      goto failed;
    }

  struct var_get_params params = {
    .vname = vname,
  };

  if (vpc_get (vc, &n->chunka, &params, e))
    {
      goto failed;
    }

  struct rptree_cursor *rc = (struct rptree_cursor *)vc;
  if (rptc_open (rc, params.pg0, n->p, n->lt, e))
    {
      goto failed;
    }

  t_size size = type_byte_size (&params.t);
  dest->cursor = rc;
  dest->backing = chunk_malloc (&n->chunka, 1, size, e);
  dest->dest = cbuffer_create (dest->backing, size);

  if (rptc_start_seek (rc, stride.start * size, false, e))
    {
      goto failed;
    }

  // SEEKING -> SEEKED
  while (rc->state == RPTS_SEEKING)
    {
      if (rptc_seeking_execute (rc, e))
        {
          goto failed;
        }
    }

  // TODO - convert to stride
  struct stride stub = { 0 };

  // SEEKED -> READING
  rptc_seeked_to_read (rc, &dest->dest, stub.nelems, size, stub.stride);

  return SUCCESS;

failed:

  // TODO - proper cleanup

  return e->cause_code;
}

err_t
var_bank_execute (struct var_bank *v, error *e)
{
  ASSERT (v->cursor->state == RPTS_DL_READING);
  rptc_read_execute (v->cursor, e);
  return SUCCESS;
}

err_t
var_bank_close (struct nsdb *n, struct var_bank *v, error *e)
{
  rptc_cleanup (v->cursor, e);
  slab_alloc_free (&n->slaba, v->cursor);

  return e->cause_code;
}
