#pragma once

#include <numstore/nsdb/type_reader.h>

struct nsdb_reader
{
  struct rptree_cursor *cursors;
  u32 vlen;

  struct type_reader *reader;
};
