#pragma once

#include "numstore/types/type_accessor.h"
#include <numstore/core/llist.h>

struct subtype
{
  struct string vname;
  struct type_accessor ta;
};

err_t subtype_create (struct subtype *dest, struct string vname, struct type_accessor ta, error *e);

struct subtype_list
{
  struct subtype *items;
  u32 len;
};

struct subtype_list_builder
{
  struct llnode *head;
  u32 count;
  struct chunk_alloc *temp;
  struct chunk_alloc *persistent;
};

struct ta_llnode
{
  struct llnode link;
  struct subtype acc;
};

void stalb_create (
    struct subtype_list_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent);

err_t stalb_accept (
    struct subtype_list_builder *builder,
    struct subtype acc,
    error *e);

err_t stalb_build (
    struct subtype_list *dest,
    struct subtype_list_builder *builder,
    error *e);
