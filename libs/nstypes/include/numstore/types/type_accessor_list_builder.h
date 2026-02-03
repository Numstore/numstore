#pragma once

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/llist.h>
#include <numstore/core/string.h>
#include <numstore/intf/types.h>
#include <numstore/types/type_accessor_builder.h>
#include <numstore/types/type_accessor_list.h>

struct type_accessor_list_builder
{
  struct llnode *head;
  u32 count;
  struct chunk_alloc *temp;
  struct chunk_alloc *persistent;
};

struct ta_llnode
{
  struct llnode link;
  struct type_accessor *acc;
};

void talb_create (
    struct type_accessor_list_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent);

err_t talb_accept (
    struct type_accessor_list_builder *builder,
    struct type_accessor *acc,
    error *e);

err_t talb_build (
    struct type_accessor_list *dest,
    struct type_accessor_list_builder *builder,
    error *e);
