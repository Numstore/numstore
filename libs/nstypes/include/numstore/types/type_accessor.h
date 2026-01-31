#pragma once

#include "numstore/types/types.h"
#include <numstore/core/cbuffer.h>

enum type_accessor_type
{
  TA_TAKE,
  TA_SELECT,
  TA_RANGE
};

struct sarray_index
{
  enum sarray_accessor_t
  {
    SA_INTEGER,
    SA_SLICE,
  } type;

  union
  {
    struct
    {
      t_size start;
      t_size step;
      t_size stop;
    } slice;

    t_size integer;
  };
};

// Type aware accessor
struct type_accessor
{
  enum type_accessor_type type;

  union
  {
    struct
    {
      const char *name;
      u32 nlen;
      struct type_accessor *next_accessor;
    } select;

    struct
    {
      struct sarray_index *indexes;
      u32 ilen;
      struct type_accessor *next_accessor;
    } range;
  };
};
