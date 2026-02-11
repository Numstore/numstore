#pragma once

#include <numstore/core/error.h>
#include <numstore/core/string.h>

struct variable
{
  struct string vname;
  struct type *dtype;
  pgno var_root;
  pgno rpt_root;
  b_size nbytes;
};

struct variable_with_cursor
{
  struct variable *var;
  struct rptree_cursor *rc;
};

err_t validate_vname (struct string vname, error *e);
