#pragma once

#include <numstore/core/stride.h>
#include <numstore/types/sarray.h>
#include <numstore/types/type_accessor.h>
#include <numstore/types/type_accessor_list.h>
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

    struct insert_stmt
    {
      struct string vname;
      b_size ofst;
      b_size nelems;
    } insert;

    struct read_stmt
    {
      struct vref_list vrefs;
      struct type_accessor_list acc;
      struct user_stride gstride;
    } read;

    struct take_stmt
    {
      struct vref_list vrefs;
      struct type_accessor_list acc;
      struct user_stride gstride;
    } take;

    struct remove_stmt
    {
      struct vref ref;
      struct user_stride gstride;
    } remove;

    struct write_stmt
    {
      struct vref vref;
      struct type_accessor_list acc;
      struct user_stride gstride;
    } write;
  };
};

///////////////////////////////////////////////////
/////////// Statement Builders

#include <numstore/types/append_stmt_builder.h>
#include <numstore/types/create_stmt_builder.h>
#include <numstore/types/delete_stmt_builder.h>
#include <numstore/types/insert_stmt_builder.h>
#include <numstore/types/read_stmt_builder.h>
#include <numstore/types/remove_stmt_builder.h>
#include <numstore/types/take_stmt_builder.h>
#include <numstore/types/write_stmt_builder.h>
