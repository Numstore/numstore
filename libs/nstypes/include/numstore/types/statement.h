#pragma once

#include "numstore/core/stride.h"
#include "numstore/types/sarray.h"
#include "numstore/types/type_accessor.h"
#include "numstore/types/types.h"

struct vref
{
  const char *name;
  const char *ref;
};

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
    struct
    {
      const char *vname;
      struct type vtype;
    } create;

    struct
    {
      const char *vname;
    } delete;

    struct
    {
      const char *vname;
      b_size ofst;
      b_size nelems;
    } insert;

    struct
    {
      struct vref *vrefs;
      u32 vrlen;

      struct type_accessor *accs;
      u32 acclen;

      struct user_stride gstride;
    } read;

    struct
    {
      struct vref *vrefs;
      u32 vrlen;

      struct type_accessor *accs;
      u32 acclen;

      struct user_stride gstride;
    } take;

    struct
    {
      struct vref ref;
      struct user_stride gstride;
    } remove;

    struct
    {
      struct vref vrefs;

      struct type_accessor *accs;
      u32 acclen;

      struct user_stride gstride;
    } write;
  };
};
