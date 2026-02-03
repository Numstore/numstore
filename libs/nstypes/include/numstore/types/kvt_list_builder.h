#pragma once

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/llist.h>
#include <numstore/core/string.h>
#include <numstore/types/types.h>

struct kvt_list
{
  u16 len;
  struct string *keys;
  struct type *types;
};

struct kv_llnode
{
  struct string key;
  struct type value;
  struct llnode link;
};

struct kvt_list_builder
{
  struct llnode *head;

  u16 klen;
  u16 tlen;

  struct chunk_alloc *temp;       // worker data
  struct chunk_alloc *persistent; // persistent memory data
};

void kvlb_create (
    struct kvt_list_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent);

err_t kvlb_accept_key (struct kvt_list_builder *ub, struct string key, error *e);
err_t kvlb_accept_type (struct kvt_list_builder *eb, struct type t, error *e);

err_t kvlb_build (struct kvt_list *dest, struct kvt_list_builder *eb, error *e);
