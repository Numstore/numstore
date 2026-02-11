#include "numstore/core/error.h"
#include "numstore/core/slab_alloc.h"
#include "numstore/rptree/rptree_cursor.h"
#include <numstore/datasource/rptc_ds.h>

err_t
rptds_init (struct rptc_ds *r, struct pager *p, struct lockt *lt, error *e)
{
  slab_alloc_init (&r->alloc, sizeof (struct rptree_cursor), 512);
  r->p = p;
  r->lt = lt;
  return SUCCESS;
}

void
rptds_free (struct rptc_ds *r)
{
  // TODO - free all cursors - maybe don't callers should free them
  slab_alloc_destroy (&r->alloc);
}

struct rptree_cursor *
rptds_open (struct rptc_ds *r, const struct variable *v, error *e)
{
  struct rptree_cursor *ret = slab_alloc_alloc (&r->alloc, e);
  if (ret == NULL)
    {
      return NULL;
    }

  if (rptc_open (ret, v->rpt_root, r->p, r->lt, e))
    {
      slab_alloc_free (&r->alloc, ret);
      return NULL;
    }

  return ret;
}

err_t
rptds_close (struct rptc_ds *r, struct rptree_cursor *c, error *e)
{
  rptc_cleanup (c, e);
  slab_alloc_free (&r->alloc, c);
  return e->cause_code;
}
