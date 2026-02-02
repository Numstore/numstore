#pragma once

#include <numstore/core/cbuffer.h>
#include <numstore/intf/types.h>

struct multi_cbuffer
{
  struct cbuffer *buffers;
  u32 len;
};
