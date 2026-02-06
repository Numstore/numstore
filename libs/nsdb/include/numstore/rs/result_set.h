#pragma once

#include <numstore/core/cbuffer.h>
#include <numstore/types/types.h>

#include <numstore/rs/fdrs.h>
#include <numstore/rs/rptrs.h>
#include <numstore/rs/trfmrs.h>

struct result_set
{
  enum rs_type
  {
    RS_FILE,
    RS_DB,
    RS_TRANSFORM,
  } type;

  union
  {
    struct fdrs f;
    struct rptrs r;
    struct trfmrs t;
  };

  struct type *out_type; // Output type of this result set
  struct cbuffer output; // Output buffer to write data onto
  u8 data[];             // Backing data for output
};
