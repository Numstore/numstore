#pragma once

#include <numstore/intf/types.h>

enum ta_type
{
  TA_TAKE,
  TA_SELECT,
  TA_RANGE,
};

struct var_bank
{
  struct rptree_cursor *src;
  struct cbuffer dest;
};

struct wrapped_byte_accessor
{
  u32 vidx;
  struct byte_accessor *ba;
  struct cbuffer dest;
};

struct multi_byte_accessor
{
  struct var_bank *banks;
  u32 blen;

  struct wrapped_byte_accessor *accs;
  u32 alen;

  struct cbuffer dest;
};
