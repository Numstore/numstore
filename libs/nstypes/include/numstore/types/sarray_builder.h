#pragma once

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/llist.h>
#include <numstore/intf/types.h>

////////////////////////////////////////////////////////////
/// Builder

struct dim_llnode
{
  u32 dim;
  struct llnode link;
};

struct sarray_builder
{
  struct llnode *head;
  struct type *type;

  struct chunk_alloc *temp;
  struct chunk_alloc *persistent;
};

// Forward declaration for sarray_t (defined in sarray.h)
struct sarray_t;
struct type;

void sab_create (struct sarray_builder *dest, struct chunk_alloc *temp, struct chunk_alloc *persistent);
err_t sab_accept_dim (struct sarray_builder *eb, u32 dim, error *e);
err_t sab_accept_type (struct sarray_builder *eb, struct type type, error *e);
err_t sab_build (struct sarray_t *persistent, struct sarray_builder *eb, error *e);
