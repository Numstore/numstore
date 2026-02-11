#pragma once

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/types/statement.h>
#include <numstore/types/type_ref.h>

err_t compile_statement (
    struct statement *dest,
    const char *text,
    struct chunk_alloc *dalloc,
    error *e);

err_t compile_type (
    struct type *dest,
    const char *text,
    struct chunk_alloc *dalloc,
    error *e);

err_t compile_stride (
    struct user_stride *dest,
    const char *text,
    error *e);

err_t compile_type_ref (
    struct type_ref *dest,
    const char *text,
    struct chunk_alloc *dalloc,
    error *e);
