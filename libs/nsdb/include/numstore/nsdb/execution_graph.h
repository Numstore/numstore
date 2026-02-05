#pragma once

#include <numstore/core/cbuffer.h>
#include <numstore/intf/types.h>

struct execution_graph
{
};

struct source
{
  void (*read_data) (struct cbuffer *dest, void *ctx);
  struct cbuffer *source;
  void *ctx;
};

struct sink
{
  struct cbuffer *input;
};
