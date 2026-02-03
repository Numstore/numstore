#pragma once

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/string.h>
#include <numstore/intf/types.h>

// Forward declaration
struct insert_stmt;

////////////////////////////////////////////////////////////
/// Builder

struct insert_builder
{
  struct string vname;
  b_size ofst;
  b_size nelems;
  struct chunk_alloc *persistent;
};

void inb_create (struct insert_builder *dest, struct chunk_alloc *persistent);

err_t inb_accept_vname (struct insert_builder *dest, struct string vname, error *e);
err_t inb_accept_ofst (struct insert_builder *dest, b_size ofst, error *e);
err_t inb_accept_nelems (struct insert_builder *dest, b_size nelems, error *e);

err_t inb_build (struct insert_stmt *dest, struct insert_builder *builder, error *e);
