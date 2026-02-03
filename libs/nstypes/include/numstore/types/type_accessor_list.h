#pragma once

#include <numstore/intf/types.h>

struct type_accessor_list
{
  struct string *vnames;
  struct type_accessor *items;
  u32 len;
};
