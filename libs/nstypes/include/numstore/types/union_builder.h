#pragma once

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/types/kvt_list_builder.h>

// Forward declaration
struct union_t;

////////////////////////////////////////////////////////////
/// Builder

struct union_builder
{
  struct chunk_alloc *persistent;
};

void unb_create (struct union_builder *dest, struct chunk_alloc *persistent);

err_t unb_accept_kvt_list (struct union_builder *builder, struct kvt_list list, error *e);

err_t unb_build (struct union_t *dest, struct union_builder *builder, struct kvt_list list, error *e);
