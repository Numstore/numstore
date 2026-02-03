#pragma once

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/stride.h>
#include <numstore/types/statement.h>
#include <numstore/types/vref_list_builder.h>

struct remove_builder
{
  struct vref vref;
  struct user_stride gstride;
  bool has_gstride;
  struct chunk_alloc *persistent;
};

void rmb_create (struct remove_builder *dest, struct chunk_alloc *persistent);

err_t rmb_accept_vref (struct remove_builder *builder, struct vref ref, error *e);

err_t rmb_accept_stride (struct remove_builder *builder, struct user_stride stride, error *e);

err_t rmb_build (struct remove_stmt *dest, struct remove_builder *builder, error *e);
