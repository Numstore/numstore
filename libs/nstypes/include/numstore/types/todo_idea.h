#pragma once

#include <numstore/intf/types.h>

enum ta_type
{
  TA_TAKE,
  TA_SELECT,
  TA_RANGE,
};

struct type_accessor
{
  enum ta_type type;

  union
  {
    struct
    {
      const char *select;
      struct type_aware_accessor *sub_ta;
    } select;

    struct
    {
      t_size start;
      t_size step;
      t_size stop;
      struct type_aware_accessor *sub_ta;
    } range;
  };
};

struct byte_accessor
{
  enum ta_type type;

  union
  {
    struct
    {
      t_size size;
    } take;

    struct
    {
      t_size bofst;
      t_size size;
      struct byte_accessor *sub_ba;
    } select;

    struct
    {
      t_size bofst;
      t_size size;
      t_size stride;
      t_size nelems;
      struct byte_accessor *sub_ba;
    } range;
  };
};

err_t ta_to_ba (struct byte_accessor *dest, struct type_accessor *src, struct type reftype, error *e);

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
