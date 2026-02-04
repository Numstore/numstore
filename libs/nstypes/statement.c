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
  err_t_wrap (validate_vname (vname, e), e);

  dest->type = ST_INSERT;
  dest->insert = (struct insert_stmt){
    .vname = vname,
    .ofst = ofst,
    .nelems = nelems,
  };
  return SUCCESS;
}

err_t
appst_create (struct statement *dest, struct string vname, b_size nelems, error *e)
{
  err_t_wrap (validate_vname (vname, e), e);

  dest->type = ST_APPEND;
  dest->append = (struct append_stmt){
    .vname = vname,
    .nelems = nelems,
  };
  return SUCCESS;
}

err_t
redst_create (struct statement *dest, struct vref_list vrefs, struct subtype_list acc, struct user_stride gstride, error *e)
{
  dest->type = ST_READ;
  dest->read = (struct read_stmt){
    .vrefs = vrefs,
    .acc = acc,
  };
  return SUCCESS;
}

err_t
takst_create (struct statement *dest, struct vref_list vrefs, struct subtype_list acc, struct user_stride gstride, error *e)
{
  dest->type = ST_TAKE;
  dest->take = (struct take_stmt){
    .vrefs = vrefs,
    .acc = acc,
  };
  return SUCCESS;
}

err_t
remst_create (struct statement *dest, struct vref ref, struct user_stride gstride, error *e)
{
  dest->type = ST_REMOVE;
  return SUCCESS;
}

err_t
wrtst_create (struct statement *dest, struct vref_list vrefs, struct subtype_list acc, struct user_stride gstride, error *e)
{
  dest->type = ST_WRITE;
  dest->write = (struct write_stmt){
    .vrefs = vrefs,
    .acc = acc,
  };
  return SUCCESS;
}
