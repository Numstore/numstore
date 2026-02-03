#pragma once

#include <numstore/core/cbuffer.h>
#include <numstore/core/slab_alloc.h>
#include <numstore/core/stride.h>
#include <numstore/nsdb/_nsdb.h>
#include <numstore/rptree/rptree_cursor.h>
#include <numstore/types/type_accessor.h>

struct var_bank
{
  struct rptree_cursor *cursor;
  struct cbuffer dest;
  u8 *backing;
};

err_t var_bank_open (
    struct nsdb *n,
    struct var_bank *dest,
    struct string vname,
    struct stride stride,
    error *e);
err_t var_bank_execute (struct var_bank *v, error *e);
err_t var_bank_close (struct nsdb *n, struct var_bank *v, error *e);

struct multi_var_bank
{
  struct var_bank *vars;
  u32 vlen;
};
