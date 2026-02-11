#include "numstore/core/error.h"
#include "numstore/datasource/rptc_ds.h"
#include "numstore/datasource/var_ds.h"
#include <numstore/repository/nsdb_rp.h>

err_t
nsdb_rp_init (struct nsdb_rp *dest, struct pager *p, struct lockt *lt, error *e)
{
  if (rptds_init (&dest->r, p, lt, e))
    {
      return e->cause_code;
    }

  if (vars_init (&dest->v, p, e))
    {
      rptds_free (&dest->r);
      return e->cause_code;
    }

  return SUCCESS;
}

void
nsdb_rp_free (struct nsdb_rp *r)
{
  rptds_free (&r->r);
  vars_close (&r->v);
}

// Fetch a new read cursor for this variable
struct rptree_cursor *
nsdb_rp_open_cursor (struct nsdb_rp *r, struct string vname, error *e)
{
  // Fetch the variable for this vname
  const struct variable *var = vars_get (&r->v, vname, e);
  if (var == NULL)
    {
      return NULL;
    }

  // Fetch the cursor for this variable
  struct rptree_cursor *ret = rptds_open (&r->r, var, e);
  if (ret == NULL)
    {
      return NULL;
    }

  // Release the variable - we don't need it anymore
  if (vars_free (&r->v, var, e))
    {
      rptds_close (&r->r, ret, e);
      return NULL;
    }

  return ret;
}

err_t
nsdb_rp_close_cursor (struct nsdb_rp *r, struct rptree_cursor *c, error *e)
{
  return rptds_close (&r->r, c, e);
}

// Fetch variable information for this variable
const struct variable *
nsdb_rp_get_variable (struct nsdb_rp *r, struct string vname, error *e)
{
  return vars_get (&r->v, vname, e);
}

err_t
nsdb_rp_free_variable (struct nsdb_rp *r, const struct variable *var, error *e)
{
  return vars_free (&r->v, var, e);
}

// Variable mutation
err_t
nsdb_rp_create (struct nsdb_rp *v, struct txn *tx, struct string vname, struct type *t, error *e)
{
  return vars_create (&v->v, tx, vname, t, e);
}

err_t
nsdb_rp_update (struct nsdb_rp *v, struct txn *tx, const struct variable *var, pgno newpg, b_size nbytes, error *e)
{
  return vars_update (&v->v, tx, var, newpg, nbytes, e);
}

err_t
nsdb_rp_delete (struct nsdb_rp *v, struct txn *tx, struct string vname, error *e)
{
  return vars_delete (&v->v, tx, vname, e);
}
