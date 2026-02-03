#pragma once

#include "numstore/nsdb/multi_var_bank.h"
#include "numstore/types/type_accessor.h"
#include <numstore/core/cbuffer.h>
#include <numstore/core/slab_alloc.h>
#include <numstore/rptree/rptree_cursor.h>
#include <numstore/types/byte_accessor.h>

struct ba_bank
{
  struct byte_accessor acc;
  u32 bank_id;
};

err_t ba_bank_init (
    struct ba_bank *dest,
    struct string valias,
    struct type_accessor *src,
    struct vref_list *vbank,
    struct multi_var_bank *mvbank,
    error *e);
