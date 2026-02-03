#include "numstore/rptree/_read.h"
#include <numstore/core/cbuffer.h>
#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/slab_alloc.h>
#include <numstore/nsdb/_nsdb.h>
#include <numstore/nsdb/var_bank.h>
#include <numstore/rptree/rptree_cursor.h>
#include <numstore/var/var_cursor.h>

err_t
var_bank_open (
    struct nsdb *n,
    struct var_bank *dest,
    struct string vname,
    struct stride stride,
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

  struct chunk_alloc temp;
  chunk_alloc_create_default (&temp);

  struct var_get_params params = {
    .vname = vname,
  };

  if (vpc_get (vc, &temp, &params, e))
    {
      goto failed;
    }

  struct rptree_cursor *rc = (struct rptree_cursor *)vc;
  if (rptc_open (rc, params.pg0, n->p, n->lt, e))
    {
      goto failed;
    }

  chunk_alloc_free_all (&temp);

  t_size size = type_byte_size (&params.t);
  dest->cursor = rc;
  dest->backing = chunk_malloc (&n->chunka, 1, size, e);
  dest->dest = cbuffer_create (dest->backing, size);

  return SUCCESS;

failed:

  // TODO - proper cleanup

  return e->cause_code;
}

err_t
var_bank_execute (struct var_bank *v, error *e)
{
  if (v->cursor->state == RPTS_SEEKING)
    {
    }
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
