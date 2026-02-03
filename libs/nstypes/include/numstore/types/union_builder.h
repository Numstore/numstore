#pragma once

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/types/kvt_list_builder.h>
#include <numstore/types/struct.h>

struct union_builder
{
  struct kvt_list list;
  bool has_list;
};

void unb_create (struct union_builder *dest, struct chunk_alloc *persistent);

err_t unb_accept_kvt_list (struct union_builder *builder, struct kvt_list list, error *e);

err_t unb_build (struct union_t *dest, struct union_builder *builder, error *e);
