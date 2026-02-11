#include "numstore/core/assert.h"
#include "numstore/core/error.h"
#include <numstore/rs/result_set.h>
#include <numstore/rs/rptrs.h>

struct result_set *
rptrs_create (
    struct rptree_cursor *rc,
    struct chunk_alloc *persistent,
    struct user_stride stride,
    struct type *t,
    error *e)
{
  // Resolve stride for this variable
  t_size size = type_byte_size (t);
  if (rc->total_size % size != 0)
    {
      error_causef (
          e, ERR_CORRUPT,
          "Type with size: %" PRt_size " has %" PRb_size " bytes, "
          "which is not a multiple of it's type size",
          size, rc->total_size);
      return NULL;
    }

  struct stride st;
  if (stride_resolve (&st, stride, rc->total_size / size, e))
    {
      return NULL;
    }

  // Begin seek
  if (rptc_start_seek (rc, st.start, false, e))
    {
      return NULL;
    }

  // Allocate return value
  struct result_set *ret = chunk_malloc (persistent, 1, sizeof (struct result_set) + size, e);
  if (ret == NULL)
    {
      return NULL;
    }

  ret->out_type = t;
  ret->output = cbuffer_create (ret->data, size);
  ret->type = RS_DB;
  ret->r.eof = rc->state == RPTS_UNSEEKED;
  ret->r.cursor = rc;
  ret->r.s = st;

  return ret;
}

err_t
rptrs_execute (struct result_set *rs, error *e)
{
  switch (rs->r.cursor->state)
    {
    case RPTS_SEEKING:
      {
        return rptc_seeking_execute (rs->r.cursor, e);
      }
    case RPTS_SEEKED:
      {
        rptc_seeked_to_read (
            rs->r.cursor,
            &rs->output,
            rs->r.s.nelems,
            type_byte_size (rs->out_type),
            rs->r.s.stride);
        return SUCCESS;
      }
    case RPTS_DL_READING:
      {
        return rptc_read_execute (rs->r.cursor, e);
      }
    case RPTS_UNSEEKED:
      {
        rs->r.eof = true;
        return SUCCESS;
      }
    default:
      {
        UNREACHABLE ();
      }
    }
}
