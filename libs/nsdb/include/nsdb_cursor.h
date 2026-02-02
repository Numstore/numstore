#pragma once

#include "numstore/core/cbuffer.h"
#include "numstore/core/slab_alloc.h"
#include "numstore/types/type_accessor.h"
#include <numstore/rptree/rptree_cursor.h>

struct var_bank
{
  struct rptree_cursor *cursor;
  struct cbuffer dest;
  u8 *backing;
};

struct multi_var_bank
{
  struct var_bank *vars;
  u32 vlen;
};

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

struct nsdb_cursor
{
  struct multi_var_bank variables;
  struct multi_ta_bank accessors;
  struct cbuffer dest;
  void *backing;

  struct slab_alloc alloc;
};

err_t nsdb_cursor_init ();
