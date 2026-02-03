#pragma once

#include <numstore/core/cbuffer.h>
#include <numstore/core/slab_alloc.h>
#include <numstore/nsdb/ba_bank.h>
#include <numstore/nsdb/multi_ba_bank.h>
#include <numstore/nsdb/multi_var_bank.h>
#include <numstore/nsdb/var_bank.h>
#include <numstore/rptree/rptree_cursor.h>
#include <numstore/types/statement.h>
#include <numstore/types/type_accessor.h>

struct nsdb_read_cursor
{
  struct multi_var_bank variables;
  struct multi_ba_bank accessors;
  struct cbuffer dest;
  void *backing;
};

err_t nsdbrc_open (
    struct nsdb *n,
    struct nsdb_read_cursor *dest,
    struct read_stmt *stmt,
    error *e);

err_t nsdbrc_execute (struct nsdb_read_cursor *dest, error *e);

err_t nsdbrc_close (struct nsdb_read_cursor *dest, error *e);
