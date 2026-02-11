#pragma once

#include "numstore/core/assert.h"
#include "numstore/types/type_ref.h"
#include <numstore/types/subtype.h>
#include <numstore/types/types.h>

struct statement
{
  enum stmnt_type
  {
    ST_CREATE,
    ST_DELETE,
    ST_INSERT,
    ST_READ,
    ST_REMOVE,
  } type;

  union
  {
    struct create_stmt
    {
      struct string vname;
      struct type vtype;
    } create;

    struct delete_stmt
    {
      struct string vname;
    } delete;

    struct insert_stmt
    {
      struct string vname;
      sb_size ofst;   // -1 if not present
      sb_size nelems; // -1 if not present
    } insert;

    struct read_stmt
    {
      struct type_ref tr;
      struct user_stride str;
    } read;

    struct remove_stmt
    {
      struct type_ref tr;
      struct user_stride str;
    } remove;
  };
};

HEADER_FUNC bool
stmt_requires_txn (enum stmnt_type st)
{
  switch (st)
    {
    case ST_CREATE:
    case ST_DELETE:
    case ST_INSERT:
    case ST_REMOVE:
      {
        return true;
      }
    case ST_READ:
      {
        return false;
      }
    }
  UNREACHABLE ();
}

err_t crtst_create (struct statement *dest, struct string vname, struct type t, error *e);
err_t dltst_create (struct statement *dest, struct string vname, error *e);
err_t redst_create (struct statement *dest, struct type_ref ref, struct user_stride gstride, error *e);

// TODO
err_t insst_create (struct statement *dest, struct string vname, sb_size ofst, sb_size nelems, error *e);
err_t takst_create (struct statement *dest, struct type_ref ref, struct user_stride gstride, error *e);
err_t remst_create (struct statement *dest, struct type_ref ref, struct user_stride gstride, error *e);

bool statement_equal (const struct statement *left, const struct statement *right);
