#pragma once

#include <numstore/core/error.h>
#include <numstore/core/string.h>

struct variable
{
  struct string vname;
  struct type *dtype;
  pgno root;
  b_size nbytes;
};

err_t validate_vname (struct string vname, error *e);
