#include <numstore/types/subtype.h>

err_t
subtype_create (struct subtype *dest, struct string vname, struct type_accessor ta, error *e)
{
  *dest = (struct subtype){
    .vname = vname,
    .ta = ta,
  };
  return SUCCESS;
}

bool
subtype_equal (const struct subtype *left, const struct subtype *right)
{
  return string_equal (left->vname, right->vname) && type_accessor_equal (left->ta, right->ta);
}
