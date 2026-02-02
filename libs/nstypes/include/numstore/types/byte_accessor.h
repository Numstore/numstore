#pragma once

#include <numstore/core/cbuffer.h>
#include <numstore/types/type_accessor.h>
#include <numstore/types/types.h>

struct byte_accessor
{
  enum ta_type type;
  t_size size;

  union
  {
    struct select_ba
    {
      t_size bofst;
      struct byte_accessor *sub_ba;
    } select;

    struct range_ba
    {
      t_size bofst;
      t_size stride;
      t_size nelems;
      struct byte_accessor *sub_ba;
    } range;
  };
};

err_t type_to_byte_accessor (
    struct byte_accessor *dest,
    struct type_accessor *src,
    struct type *reftype,
    error *e);

void ta_memcpy_from (
    struct cbuffer *dest,
    struct cbuffer *src,
    struct byte_accessor *acc,
    u32 acclen);

void ta_memcpy_to (
    u8 *dest,
    struct cbuffer *src,
    struct byte_accessor *acc,
    u32 acclen);
