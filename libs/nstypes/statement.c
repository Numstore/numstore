#include "numstore/types/variables.h"
#include <numstore/types/statement.h>

err_t
crtst_create (struct statement *dest, struct string vname, struct type t, error *e)
{
  err_t_wrap (validate_vname (vname, e), e);

  dest->type = ST_CREATE;
  dest->create = (struct create_stmt){
    .vname = vname,
    .vtype = t,
  };
  return SUCCESS;
}

err_t
dltst_create (struct statement *dest, struct string vname, error *e)
{
  err_t_wrap (validate_vname (vname, e), e);

  dest->type = ST_DELETE;
  dest->delete = (struct delete_stmt){
    .vname = vname,
  };
  return SUCCESS;
}

err_t
insst_create (struct statement *dest, struct string vname, b_size ofst, b_size nelems, error *e)
{
  panic ("TODO");
}

err_t
appst_create (struct statement *dest, struct string vname, b_size nelems, error *e)
{
  panic ("TODO");
}

err_t
takst_create (struct statement *dest, struct vref_list vrefs, struct subtype_list acc, struct user_stride gstride, error *e)
{
  panic ("TODO");
}

err_t
remst_create (struct statement *dest, struct vref ref, struct user_stride gstride, error *e)
{
  panic ("TODO");
}

err_t
wrtst_create (struct statement *dest, struct vref_list vrefs, struct subtype_list acc, struct user_stride gstride, error *e)
{
  panic ("TODO");
}

err_t
redst_create (struct statement *dest, struct vref_list vrefs, struct subtype_list acc, struct user_stride gstride, error *e)
{
  panic ("TODO");
}

bool
statement_equal (const struct statement *left, const struct statement *right)
{
  panic ("TODO");
  return false;
}
