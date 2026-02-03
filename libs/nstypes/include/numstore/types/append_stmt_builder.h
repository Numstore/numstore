#pragma once

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/string.h>
#include <numstore/intf/types.h>

// Forward declaration (builds to insert_stmt)
struct insert_stmt;

////////////////////////////////////////////////////////////
/// Builder

struct append_builder
{
  struct string vname;
  b_size nelems;
  struct chunk_alloc *persistent;
};

void apb_create (struct append_builder *dest, struct chunk_alloc *persistent);

err_t apb_accept_vname (struct append_builder *dest, struct string vname, error *e);
err_t apb_accept_nelems (struct append_builder *dest, b_size nelems, error *e);

err_t apb_build (struct insert_stmt *dest, struct append_builder *builder, error *e);
