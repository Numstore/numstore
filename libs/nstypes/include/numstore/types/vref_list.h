#pragma once

#include <numstore/types/vref.h>

struct vref_list
{
  struct vref *items;
  u32 len;
};
