#pragma once

#include "numstore/core/stride.h"
#include "numstore/types/types.h"
#include <numstore/rptree/rptree_cursor.h>

struct rptrs
{
  bool eof;
  struct stride s;
  struct rptree_cursor *cursor;
};

struct result_set *rptrs_create (
    struct rptree_cursor *rc,
    struct chunk_alloc *persistent,
    struct user_stride stride,
    struct type *t,
    error *e);

err_t rptrs_execute (struct result_set *rs, error *e);
