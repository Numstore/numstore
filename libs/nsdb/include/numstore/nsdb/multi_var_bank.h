#pragma once

#include "numstore/types/vref_list.h"
#include <numstore/core/cbuffer.h>
#include <numstore/core/slab_alloc.h>
#include <numstore/core/stride.h>
#include <numstore/nsdb/nsdb.h>
#include <numstore/nsdb/var_bank.h>
#include <numstore/rptree/rptree_cursor.h>
#include <numstore/types/type_accessor.h>

struct multi_var_bank
{
  struct var_bank *vars;
  u32 vlen;
};

err_t mvar_bank_open_all (
    struct nsdb *n,
    struct multi_var_bank *dest,
    struct vref_list vrefs,
    struct user_stride stride,
    error *e);
err_t mvar_bank_execute (struct multi_var_bank *v, error *e);
err_t mvar_bank_close (struct nsdb *n, struct multi_var_bank *v, error *e);
