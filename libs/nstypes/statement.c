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

bool
statement_equal (const struct statement *left, const struct statement *right)
{
  if (left->type != right->type)
    {
      return false;
    }

  switch (left->type)
    {
    case ST_CREATE:
      {
        return string_equal (left->create.vname, right->create.vname)
               && type_equal (&left->create.vtype, &right->create.vtype);
      }
    case ST_DELETE:
      {
        return string_equal (left->delete.vname, right->delete.vname);
      }
    case ST_INSERT:
      {
        return string_equal (left->insert.vname, right->insert.vname)
               && left->insert.ofst == right->insert.ofst && left->insert.nelems == right->insert.nelems;
      }
    case ST_APPEND:
      {
        return string_equal (left->append.vname, right->append.vname)
               && left->append.nelems == right->append.nelems;
      }
    case ST_READ:
      {
        return vref_list_equal (&left->read.vrefs, &right->read.vrefs)
               && subtype_list_equal (&left->read.acc, &right->read.acc)
               && user_stride_equal (&left->read.gstride, &right->read.gstride);
      }
    case ST_TAKE:
      {
        return vref_list_equal (&left->take.vrefs, &right->take.vrefs)
               && subtype_list_equal (&left->take.acc, &right->take.acc)
               && user_stride_equal (&left->take.gstride, &right->take.gstride);
      }
    case ST_REMOVE:
      {
        return vref_equal (&left->remove.ref, &right->remove.ref)
               && user_stride_equal (&left->remove.gstride, &right->remove.gstride);
      }
    case ST_WRITE:
      {
        return vref_list_equal (&left->write.vrefs, &right->write.vrefs)
               && subtype_list_equal (&left->write.acc, &right->write.acc)
               && user_stride_equal (&left->write.gstride, &right->write.gstride);
      }
    }

  return false;
}
