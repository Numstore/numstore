#pragma once

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/string.h>
#include <numstore/types/statement.h>

struct delete_builder
{
  struct string vname;
  struct chunk_alloc *persistent;
};

void dlb_create (struct delete_builder *dest, struct chunk_alloc *persistent);

err_t dlb_accept_vname (struct delete_builder *dest, struct string vname, error *e);

err_t dlb_build (struct delete_stmt *dest, struct delete_builder *builder, error *e);
