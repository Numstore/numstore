#pragma once

#include <numstore/types/vref.h>

struct vref_list
{
  struct vref *items;
  u32 len;
};

i32 vrefl_find_variable (struct vref_list *list, struct string vname);
