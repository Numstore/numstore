#pragma once

#include <numstore/core/cbuffer.h>
#include <numstore/core/slab_alloc.h>
#include <numstore/nsdb/nsdb.h>
#include <numstore/rptree/rptree_cursor.h>
#include <numstore/types/statement.h>
#include <numstore/types/type_accessor.h>
#include <numstore/var/var_cursor.h>

struct nsdb_read_cursor
{
  // A bank of variables and their cursors
  struct
  {
    union cursor **cursors;
    struct cbuffer *singles;
    struct type *types;
    u32 len;

    u8 *_data;
    struct stride gstride;
  } variables;

  // A bank of accessors which point to each variable
  struct
  {
    struct byte_accessor *acc;
    u32 *vid;
    u32 len;
  } accessors;

  struct cbuffer dest;

  // Allocator for internals
  struct chunk_alloc alloc;
};

struct nsdb_read_cursor *nsdbrc_open (struct nsdb *n, struct read_stmt *stmt, error *e);

err_t nsdbrc_execute (struct nsdb_read_cursor *dest, error *e);

err_t nsdbrc_close (struct nsdb *n, struct nsdb_read_cursor *dest, error *e);
