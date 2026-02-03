#pragma once

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/stride.h>
#include <numstore/types/statement.h>
#include <numstore/types/type_accessor_list_builder.h>
#include <numstore/types/vref_list_builder.h>

struct read_builder
{
  struct vref_list vrefs;
  struct type_accessor_list acc;
  struct user_stride gstride;
  bool has_gstride;
};

void rdb_create (
    struct read_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent);

err_t rdb_accept_vref_list (
    struct read_builder *builder,
    struct vref_list list,
    error *e);

err_t rdb_accept_accessor_list (
    struct read_builder *builder,
    struct type_accessor_list *acc,
    error *e);

err_t rdb_accept_stride (
    struct read_builder *builder,
    struct user_stride stride,
    error *e);

err_t rdb_build (struct read_stmt *dest, struct read_builder *builder, error *e);
