#pragma once

#include "numstore/core/chunk_alloc.h"
#include <numstore/core/adptv_hash_table.h>
#include <numstore/core/clock_allocator.h>
#include <numstore/core/hash_table.h>
#include <numstore/core/slab_alloc.h>
#include <numstore/types/type_accessor.h>
#include <numstore/types/types.h>
#include <numstore/types/variables.h>

struct vbank
{
  struct htable *table;
  struct chunk_alloc *alloc;
};

err_t vbank_init (struct vbank *dest, struct chunk_alloc *alloc, error *e);
void vbank_close (struct vbank *v);

err_t vbank_insert (
    struct vbank *v,
    struct string vname,
    struct string alias,
    struct type *type,
    error *e);
struct var_with_alias *vbank_get (struct vbank *v, struct string alias);
