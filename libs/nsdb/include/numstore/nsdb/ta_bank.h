#pragma once

#include <numstore/core/cbuffer.h>
#include <numstore/core/slab_alloc.h>
#include <numstore/rptree/rptree_cursor.h>
#include <numstore/types/type_accessor.h>

struct ta_bank
{
  struct type_accessor acc;
  u32 bank_id;
};

struct multi_ta_bank
{
  struct ta_bank *tas;
  u32 tlen;
};
