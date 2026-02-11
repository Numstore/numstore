#pragma once

#include "numstore/types/type_ref.h"
#include <numstore/core/cbuffer.h>
#include <numstore/types/types.h>

#include <numstore/rs/rptrs.h>
#include <numstore/rs/trfmrs.h>

struct result_set
{
  enum rs_type
  {
    RS_DB,
    RS_TRANSFORM,
  } type;

  union
  {
    struct rptrs r;
    struct trfmrs t;
  };

  struct type *out_type; // Output type of this result set
  struct cbuffer output; // Output buffer to write data onto
  u8 data[];             // Backing data for output
};

err_t rs_execute (struct result_set *rs, error *e);
