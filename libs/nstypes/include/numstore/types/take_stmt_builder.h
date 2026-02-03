#pragma once

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/stride.h>
#include <numstore/types/statement.h>
#include <numstore/types/type_accessor_list_builder.h>
#include <numstore/types/vref_list_builder.h>

struct take_builder
{
  struct vref_list vrefs;
  struct type_accessor_list accs;
  struct user_stride gstride;
  bool has_gstride;
};

void tkb_create (
    struct take_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent);

err_t tkb_accept_vref_list (
    struct take_builder *builder,
    struct vref_list vrefs,
    error *e);

err_t tkb_accept_accessor_list (
    struct take_builder *builder,
    struct type_accessor_list *acc,
    error *e);

err_t tkb_accept_stride (
    struct take_builder *builder,
    struct user_stride stride,
    error *e);

err_t tkb_build (struct take_stmt *dest, struct take_builder *builder, error *e);
