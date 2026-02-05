#pragma once

#include "numstore/types/vbank.h"
#include <numstore/types/subtype.h>
#include <numstore/types/types.h>
#include <numstore/types/vref.h>

struct statement
{
  enum stmnt_type
  {
    ST_CREATE,
    ST_DELETE,
    ST_INSERT,
    ST_APPEND,
    ST_READ,
    ST_WRITE,
    ST_REMOVE,
    ST_TAKE,
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

    struct read_stmt
    {
      union
      {
        struct dbread
        {
          // Bank of active variables
          // from variable1 as foo, variable2 as bar
          struct vbank vars;

          // List of Variable accessors
          // (a.b, a.c.d, ...)
          struct subtype_list accs;

          // Global stride
          struct user_stride stride;
        } fromdb;
        struct
        {
          struct string fname;
        } fromfile;
      };
    } read;
  };
};

err_t crtst_create (struct statement *dest, struct string vname, struct type t, error *e);
err_t dltst_create (struct statement *dest, struct string vname, error *e);
err_t redst_create (struct statement *dest, struct vref_list vrefs, struct subtype_list acc, struct user_stride gstride, error *e);

// TODO
err_t insst_create (struct statement *dest, struct string vname, b_size ofst, b_size nelems, error *e);
err_t appst_create (struct statement *dest, struct string vname, b_size nelems, error *e);
err_t takst_create (struct statement *dest, struct vref_list vrefs, struct subtype_list acc, struct user_stride gstride, error *e);
err_t remst_create (struct statement *dest, struct vref ref, struct user_stride gstride, error *e);
err_t wrtst_create (struct statement *dest, struct vref_list vrefs, struct subtype_list acc, struct user_stride gstride, error *e);

bool statement_equal (const struct statement *left, const struct statement *right);
