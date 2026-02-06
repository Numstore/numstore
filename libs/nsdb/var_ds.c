#include <numstore/datasource/var_ds.h>

struct var_entry
{
  struct variable var;
  struct chunk_alloc alloc;
  struct hnode node;
};

err_t
vars_init (struct var_ds *dest, struct chunk_alloc *alloc, error *e)
{
  return SUCCESS;
}
void
vars_close (struct var_ds *v)
{
  return;
}

struct variable *
vars_get (struct var_ds *v, struct string vname, error *e)
{

  return NULL;
}
err_t
vars_unpin (struct var_ds *v, struct variable *vr, error *e)
{

  return SUCCESS;
}
struct variable *
vars_create (struct var_ds *v, struct string vname, struct type t, error *e)
{
  return NULL;
}
err_t
vars_update (struct var_ds *v, struct string vname, pgno newpg, b_size nbytes, error *e)
{

  return SUCCESS;
}
err_t
vars_delete (struct var_ds *v, struct string vname, error *e)
{

  return SUCCESS;
}
