#pragma once

#include "numstore/types/type_accessor.h"
#include <numstore/core/llist.h>

struct subtype
{
  struct string vname;
  struct type_accessor ta;
};

err_t subtype_create (struct subtype *dest, struct string vname, struct type_accessor ta, error *e);
bool subtype_equal (const struct subtype *left, const struct subtype *right);
