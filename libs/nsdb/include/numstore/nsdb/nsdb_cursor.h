#pragma once

#include "numstore/types/statement.h"
#include <numstore/core/cbuffer.h>
#include <numstore/core/slab_alloc.h>
#include <numstore/nsdb/ta_bank.h>
#include <numstore/nsdb/var_bank.h>
#include <numstore/rptree/rptree_cursor.h>
#include <numstore/types/type_accessor.h>

struct nsdb_cursor
{
  struct multi_var_bank variables;
  struct multi_ta_bank accessors;
  struct cbuffer dest;
  void *backing;

  struct slab_alloc alloc;
};

err_t nsdbc_open (
    struct nsdb_cursor *dest,
    struct slab_alloc *rpt_alloc,
    struct read_stmt *stmt,
    error *e);

err_t nsdbc_execute (struct nsdb_cursor *dest, error *e);

err_t nsdbc_close (struct nsdb_cursor *dest, error *e);
