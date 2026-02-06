#pragma once

#include "numstore/core/stride.h"
#include <numstore/core/error.h>
#include <numstore/core/string.h>
#include <numstore/intf/types.h>
#include <numstore/rs/predicate.h>

struct trfmrs
{
  // The inputs to this result set - these are typed result sets
  struct result_set **inputs;
  u32 rslen;

  // A list of
  struct byte_accessor *accs; // A list of "get this part of [input[i]]
  u32 *vnums;                 // References into [inputs]
  u32 aclen;

  struct predicate where;
};

struct trfmrsb
{
  int temp;
};

struct trfmrsb trfmrsb_create (void);
err_t trfmrsb_add_from (struct trfmrsb *b, struct result_set *rs, struct string valias, error *e);
err_t trfmrsb_add_where (struct trfmrsb *b, void *todo_where_decl, error *e);
err_t trfmrsb_add_select (struct trfmrsb *b, void *todo_type_decl, error *e);
err_t trfmrsb_add_slice (struct trfmrsb *b, struct user_stride stride, error *e);
err_t trfmrsb_build (struct result_set *dest, struct trfmrs *src, error *e);
