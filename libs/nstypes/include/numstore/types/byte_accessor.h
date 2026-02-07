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
      struct stride stride;
      struct byte_accessor *sub_ba;
    } range;
  };
};

err_t type_to_byte_accessor (
    struct byte_accessor *dest,
    struct type_accessor *src,
    struct type *reftype,
    struct chunk_alloc *dalloc, // Where to allocate data onto
    error *e);

t_size ba_byte_size (struct byte_accessor *ba);

void ba_memcpy_from (
    struct cbuffer *dest,
    struct cbuffer *src,
    struct byte_accessor *acc);

void ba_memcpy_to (
    struct cbuffer *dest,
    struct cbuffer *src,
    struct byte_accessor *acc);
