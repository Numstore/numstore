#pragma once

#include <numstore/core/cbuffer.h>
#include <numstore/types/type_accessor.h>
#include <numstore/types/types.h>

// Type unaware accessor with just stride lengths
struct byte_accessor
{
  enum type_accessor_type type;
  union
  {
    struct
    {
      t_size size;
    } take;
    struct
    {
      struct byte_accessor *query;
      t_size offset;
    } select;
    struct
    {
      struct byte_accessor *query;
      t_size start;  // Start element index
      t_size stride; // Element stride (1 = every element, 2 = every other, etc)
      t_size end;    // End element index (exclusive)
    } range;
  };
};

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
