#pragma once

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/string.h>
#include <numstore/types/statement.h>
#include <numstore/types/types.h>

struct create_builder
{
  struct string vname;
  struct type vtype;
  struct chunk_alloc *persistent;
};

void crb_create (struct create_builder *dest, struct chunk_alloc *persistent);

err_t crb_accept_vname (struct create_builder *dest, struct string vname, error *e);
err_t crb_accept_type (struct create_builder *dest, struct type t, error *e);

err_t crb_build (struct create_stmt *dest, struct create_builder *builder, error *e);
