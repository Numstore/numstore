#pragma once

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/stride.h>
#include <numstore/core/string.h>
#include <numstore/types/statement.h>
#include <numstore/types/type_accessor_list_builder.h>
#include <numstore/types/vref_list_builder.h>

struct write_builder
{
  struct vref vref;
  struct type_accessor_list acc;
  struct user_stride gstride;
  bool has_gstride;
  struct chunk_alloc *persistent;
};

void wrb_create (
    struct write_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent);

err_t wrb_accept_vref (
    struct write_builder *builder,
    struct string name,
    struct string ref,
    error *e);

err_t wrb_accept_accessor_list (
    struct write_builder *builder,
    struct type_accessor_list *acc,
    error *e);

err_t wrb_accept_stride (
    struct write_builder *builder,
    struct user_stride stride,
    error *e);

err_t wrb_build (struct write_stmt *dest, struct write_builder *builder, error *e);
