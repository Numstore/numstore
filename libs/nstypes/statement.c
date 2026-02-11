#include "numstore/core/assert.h"
#include <numstore/types/statement.h>

err_t
crtst_create (struct statement *dest, struct string vname, struct type t, error *e)
{
  *dest = (struct statement){
    .type = ST_CREATE,
    .create = {
        .vname = vname,
        .vtype = t,
    },
  };
  return SUCCESS;
}

err_t
dltst_create (struct statement *dest, struct string vname, error *e)
{
  *dest = (struct statement){
    .type = ST_DELETE,
    .delete = {
        .vname = vname,
    },
  };
  return SUCCESS;
}

err_t
redst_create (struct statement *dest, struct type_ref ref, struct user_stride gstride, error *e)
{
  *dest = (struct statement){
    .type = ST_READ,
    .read = {
        .tr = ref,
        .str = gstride,
    },
  };
  return SUCCESS;
}

err_t
insst_create (struct statement *dest, struct string vname, sb_size ofst, sb_size nelems, error *e)
{
  *dest = (struct statement){
    .type = ST_INSERT,
    .insert = {
        .vname = vname,
        .ofst = ofst,
        .nelems = nelems,
    },
  };
  return SUCCESS;
}

err_t
remst_create (struct statement *dest, struct type_ref ref, struct user_stride gstride, error *e)
{
  *dest = (struct statement){
    .type = ST_REMOVE,
    .remove = {
        .tr = ref,
        .str = gstride,
    },
  };
  return SUCCESS;
}

bool
statement_equal (const struct statement *left, const struct statement *right)
{
  if (left->type != right->type)
    return false;

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
               && left->insert.ofst == right->insert.ofst
               && left->insert.nelems == right->insert.nelems;
      }

    case ST_READ:
      {
        return type_ref_equal (left->read.tr, right->read.tr)
               && ustride_equal (left->read.str, right->read.str);
      }

    case ST_REMOVE:
      {
        return type_ref_equal (left->remove.tr, right->remove.tr)
               && ustride_equal (left->remove.str, right->remove.str);
      }
    }

  UNREACHABLE ();
}
