#pragma once

#include "numstore/compiler/expression.h"
#include "numstore/core/chunk_alloc.h"
#include "numstore/core/stride.h"
#include "numstore/intf/types.h"
#include "numstore/rptree/rptree_cursor.h"
#include "numstore/types/type_accessor.h"
#include "numstore/var/var_cursor.h"

/**
 * read (a.b as foo, b.c as bar, d.e as biz)[0:100:1000]
 *   from
 *     variable1.a.b        as a,
 *     variable1[0:10:100]  as b,
 *     variable3            as d
 *   where
 *     a.b > 10 && sum(d.c) > 100 && a.c + d.f == 5
 */
struct read_query
{
  // (a.b, b.c, d.e)
  struct
  {
    struct string vname;
    struct string alias;
    struct type_accessor acc;
  } * outputs;
  u32 plen;

  // [0:100:1000]
  struct user_stride gstride;

  /**
   *     variable1.a.b        as a,
   *     variable2[0:10:100]  as b,
   *     variable3            as d
   */
  struct
  {
    struct string vname;
    struct type_accessor acc;
    struct string alias;
  } * aliases;
  u32 alen;

  // a.b > 10 && sum(d.c) > 100 && a.c + d.f == 5
  struct expr predicate;
};
