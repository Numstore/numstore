#pragma once

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/types/kvt_list_builder.h>

// Forward declaration
struct struct_t;

////////////////////////////////////////////////////////////
/// Builder

struct struct_builder
{
  struct chunk_alloc *persistent;
};

void stb_create (struct struct_builder *dest, struct chunk_alloc *persistent);

err_t stb_accept_kvt_list (struct struct_builder *builder, struct kvt_list list, error *e);

err_t stb_build (struct struct_t *dest, struct struct_builder *builder, struct kvt_list list, error *e);
