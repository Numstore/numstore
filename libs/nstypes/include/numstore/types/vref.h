#pragma once

#include <numstore/core/llist.h>
#include <numstore/core/string.h>

struct vref
{
  struct string vname;
  struct string alias;
};

struct vref_list
{
  struct vref *items;
  u32 len;
};

i32 vrefl_find_variable (struct vref_list *list, struct string vname);

struct vref_list_builder
{
  struct llnode *head;
  u32 count;
  struct chunk_alloc *temp;
  struct chunk_alloc *persistent;
};

struct vref_llnode
{
  struct llnode link;
  struct vref ref;
};

void vrlb_create (
    struct vref_list_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent);

err_t vrlb_accept (
    struct vref_list_builder *builder,
    const char *name,
    const char *ref,
    error *e);

err_t vrlb_build (
    struct vref_list *dest,
    struct vref_list_builder *builder,
    error *e);
