#pragma once

#include "numstore/core/llist.h"
#include "numstore/core/stride.h"
#include "numstore/types/type_accessor.h"
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
  struct byte_accessor **accs; // A list of "get this part of [input[i]]
  u32 *vnums;                  // References into [inputs]
  u32 aclen;
};

struct trfmrsb_input_llnode
{
  struct result_set *rs;
  struct llnode link;
};

struct trfmrsb_ta_llnode
{
  struct byte_accessor *bacc;
  struct type t;
  struct string name;
  u32 idx;
  struct llnode link;
};

struct trfmrsb
{
  struct llnode *rs_head;
  struct llnode *ta_head;

  struct user_stride stride;
  bool stride_supplied;

  struct chunk_alloc *temp;
  struct chunk_alloc *persistent;
};

void trfmrsb_create (struct trfmrsb *dest, struct chunk_alloc *temp, struct chunk_alloc *persistent);
err_t trfmrsb_append_select (
    struct trfmrsb *b,
    struct result_set *rs,
    struct type_accessor *ta,
    struct string name,
    error *e);
err_t trfmrsb_add_slice (struct trfmrsb *b, struct user_stride stride, error *e);
struct result_set *trfmrsb_build (struct trfmrsb *src, error *e);
