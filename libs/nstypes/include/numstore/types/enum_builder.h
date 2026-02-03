#pragma once

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/llist.h>
#include <numstore/core/string.h>
#include <numstore/types/enum.h>

struct k_llnode
{
  struct string key;
  struct llnode link;
};

struct enum_builder
{
  struct llnode *head;
  struct chunk_alloc *temp;
  struct chunk_alloc *persistent;
};

void enb_create (
    struct enum_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent);

err_t enb_accept_key (struct enum_builder *eb, struct string key, error *e);
err_t enb_build (struct enum_t *persistent, struct enum_builder *eb, error *e);
