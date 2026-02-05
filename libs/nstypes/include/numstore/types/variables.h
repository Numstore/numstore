#pragma once

#include <numstore/core/error.h>
#include <numstore/core/string.h>

struct var_data
{
  struct type *vtype;
  const u8 *data;
};

struct var_with_alias
{
  struct string vname;
  struct string alias;
  struct type *vtype;
  u8 *data;
};

err_t validate_vname (struct string vname, error *e);
