#pragma once

#include <numstore/core/cbuffer.h>
#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/string.h>
#include <numstore/intf/types.h>

enum ta_type
{
  TA_TAKE,
  TA_SELECT,
  TA_RANGE
};

struct type_accessor
{
  enum ta_type type;

  union
  {
    struct select_ta
    {
      struct string key;
      struct type_accessor *sub_ta;
    } select;

    struct range_ta
    {
      t_size start;
      t_size step;
      t_size stop;
      struct type_accessor *sub_ta;
    } range;
  };
};
