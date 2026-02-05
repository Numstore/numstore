#pragma once

#include "numstore/types/type_accessor.h"
#include "numstore/types/vbank.h"
#include <numstore/types/byte_accessor.h>

typedef bool (*pred_func) (struct cbuffer **buffers, void *ctx);

struct transform_predicate
{
  // List of variable sizes
  struct cbuffer **data;
  u32 vlen;

  // List of accessors and their referenced variables
  struct byte_accessor **acc;
  u32 alen;

  // Predicate to operate on
  void *pctx;
  pred_func predicate;

  // Output
  struct cbuffer *output;
  u32 singleoutlen;
};

void trpr_init (struct transform_predicate, struct vbank *bank);
err_t trpr_add_var_source (struct string valias, struct type_accessor *acc, error *e);
err_t trpr_add_trpr_source (struct transform_predicate *tr, error *e);
void trpr_execute (struct transform_predicate *r);
