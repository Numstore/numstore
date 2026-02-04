#include "numstore/core/cbuffer.h"
#include "numstore/core/chunk_alloc.h"
#include "numstore/core/error.h"
#include "numstore/core/slab_alloc.h"
#include "numstore/core/stride.h"
#include "numstore/rptree/_read.h"
#include "numstore/rptree/rptree_cursor.h"
#include "numstore/types/byte_accessor.h"
#include "numstore/types/type_accessor.h"
#include <numstore/nsdb/nsdb_read_cursor.h>

static inline err_t
nsdbrc_open_cursor (
    struct nsdb *n,
    struct nsdb_read_cursor *rc,
    struct read_stmt *stmt,
    error *e)
{
  b_size min_arrlen = (b_size)-1;

  for (u32 i = 0; i < rc->variables.len; ++i)
    {
      // Allocate the cursor
      union cursor *c = slab_alloc_alloc (&n->slaba, e);
      if (c == NULL)
        {
          return e->cause_code;
        }
      rc->variables.cursors[i] = c;

      // Read in the variable type
      if (varc_initialize (&c->vc, n->p, e))
        {
          goto failed;
        }

      struct var_get_params params = {
        .vname = stmt->vrefs.items[i].vname,
      };

      if (vpc_get (&c->vc, &n->chunka, &params, e))
        {
          goto failed;
        }

      // Open the rptree cursor on the starting page number
      if (rptc_open (&c->rc, params.pg0, n->p, n->lt, e))
        {
          goto failed;
        }

      rc->variables.types[i] = params.t;

      // Get the smallest length array
      t_size size = type_byte_size (&params.t);
      if (params.nbytes / size < min_arrlen)
        {
          min_arrlen = params.nbytes / size;
        }
    }

  stride_resolve_expect (&rc->variables.gstride, stmt->gstride, min_arrlen);

  return SUCCESS;

failed:
  return e->cause_code;
}

static inline err_t
nsdbrc_assign_variable_cbuffers (struct nsdb_read_cursor *rc, error *e)
{
  // Get one total allocation size for all buffers
  t_size ret = 0;
  for (u32 i = 0; i < rc->variables.len; ++i)
    {
      ret += type_byte_size (&rc->variables.types[i]);
    }

  rc->variables._data = chunk_malloc (&rc->alloc, ret, 1, e);
  if (rc->variables._data == NULL)
    {
      return e->cause_code;
    }

  // Assign each cbuffer
  ret = 0;
  for (u32 i = 0; i < rc->variables.len; ++i)
    {
      t_size size = type_byte_size (&rc->variables.types[i]);
      rc->variables.singles[i] = cbuffer_create (rc->variables._data + i, size);
      ret += size;
    }

  return SUCCESS;
}

static inline err_t
nsdbrc_wind_cursors (struct nsdb_read_cursor *rc, error *e)
{
  for (u32 i = 0; i < rc->variables.len; ++i)
    {
      t_size size = type_byte_size (&rc->variables.types[i]);

      struct rptree_cursor *c = &rc->variables.cursors[i]->rc;

      if (rptc_start_seek (c, rc->variables.gstride.start * size, false, e))
        {
          goto failed;
        }

      // SEEKING -> SEEKED
      while (c->state == RPTS_SEEKING)
        {
          if (rptc_seeking_execute (c, e))
            {
              goto failed;
            }
        }

      // SEEKED -> READING
      rptc_seeked_to_read (c, &rc->variables.singles[i], rc->variables.gstride.nelems, size, rc->variables.gstride.stride);
    }

  return SUCCESS;

failed:
  return e->cause_code;
}

static err_t
nsdbrc_init_byte_accessors (
    struct nsdb_read_cursor *rc,
    struct read_stmt *stmt,
    error *e)
{
  for (u32 i = 0; i < stmt->acc.len; ++i)
    {
      i32 vid = vrefl_find_variable (&stmt->vrefs, stmt->acc.items[i].vname);
      if (vid < 0)
        {
          return error_causef (
              e, ERR_INVALID_ARGUMENT,
              "Failed to find variable: %.*s",
              stmt->acc.items[i].vname.len, stmt->acc.items[i].vname.data);
        }
      if (type_to_byte_accessor (&rc->accessors.acc[i], &stmt->acc.items[i].ta, &rc->variables.types[i], e))
        {
          goto failed;
        }
      rc->accessors.vid[i] = vid;
    }

  return SUCCESS;

failed:
  return e->cause_code;
}

struct nsdb_read_cursor *
nsdbrc_open (
    struct nsdb *n,
    struct read_stmt *stmt,
    error *e)
{
  struct nsdb_read_cursor *dest = slab_alloc_alloc (&n->dcursor_slaba, e);
  if (dest == NULL)
    {
      return NULL;
    }

  chunk_alloc_create_default (&dest->alloc);

  // Allocate memory first time
  {
    // Variables
    {
      dest->variables.len = stmt->vrefs.len;
      dest->variables.cursors = chunk_malloc (&dest->alloc, dest->variables.len, sizeof (union cursor *), e);
      dest->variables.singles = chunk_malloc (&dest->alloc, dest->variables.len, sizeof *dest->variables.singles, e);
      dest->variables.types = chunk_malloc (&dest->alloc, dest->variables.len, sizeof *dest->variables.types, e);

      if (dest->variables.cursors == NULL || dest->variables.singles == NULL || dest->variables.types == NULL)
        {
          chunk_alloc_free_all (&dest->alloc);
          return NULL;
        }
    }

    // Accessors
    {
      dest->accessors.len = stmt->acc.len;
      dest->accessors.acc = chunk_malloc (&dest->alloc, dest->accessors.len, sizeof *dest->accessors.acc, e);
      dest->accessors.vid = chunk_malloc (&dest->alloc, dest->accessors.len, sizeof *dest->accessors.vid, e);

      if (dest->accessors.acc == NULL || dest->accessors.vid == NULL)
        {
          chunk_alloc_free_all (&dest->alloc);
          return NULL;
        }
    }
  }

  // Initialize variables and cursors
  err_t_panic (nsdbrc_open_cursor (n, dest, stmt, e), e);
  err_t_panic (nsdbrc_assign_variable_cbuffers (dest, e), e);
  err_t_panic (nsdbrc_wind_cursors (dest, e), e);

  err_t_panic (nsdbrc_init_byte_accessors (dest, stmt, e), e);

  return dest;
}

static inline err_t
nsdbrc_read_cursors (struct nsdb_read_cursor *dest, error *e)
{
  for (u32 i = 0; i < dest->variables.len; ++i)
    {
      struct rptree_cursor *c = &dest->variables.cursors[i]->rc;

      switch (c->state)
        {
        case RPTS_UNSEEKED:
          {
            return error_causef (e, ERR_CORRUPT, "Early termination of variable. sizeof returned the wrong value.");
          }
        case RPTS_DL_READING:
          {
            return rptc_read_execute (c, e);
          }
        case RPTS_SEEKED:
        case RPTS_SEEKING:
        case RPTS_DL_INSERTING:
        case RPTS_DL_REMOVING:
        case RPTS_IN_REBALANCING:
        case RPTS_DL_WRITING:
        case RPTS_PERMISSIVE:
          {
            UNREACHABLE ();
          }
        }
    }

  UNREACHABLE ();
}

static inline void
nsdbrc_ba_memcpy (struct nsdb_read_cursor *rc)
{
  if (cbuffer_len (&rc->dest) != 0)
    {
      // Block on upstream
      return;
    }

  for (u32 i = 0; i < rc->variables.len; ++i)
    {
      if (cbuffer_avail (&rc->variables.singles[i]) != 0)
        {
          // Block on downstream
          return;
        }
    }

  // Do memcpy for all
  for (u32 i = 0; i < rc->accessors.len; ++i)
    {
      u32 vidx = rc->accessors.vid[i];
      ASSERT (vidx < rc->variables.len);

      struct cbuffer *src = &rc->variables.singles[vidx];
      struct byte_accessor *ba = &rc->accessors.acc[vidx];

      u32 mark = cbuffer_mark (src);
      ba_memcpy_from (&rc->dest, src, ba);
      cbuffer_reset (src, mark);
    }

  // "consume" source buffers
  for (u32 i = 0; i < rc->variables.len; ++i)
    {
      struct cbuffer *src = &rc->variables.singles[i];
      cbuffer_discard_all (src);
    }

  ASSERT (cbuffer_avail (&rc->dest) == 0);
}

err_t
nsdbrc_execute (struct nsdb_read_cursor *dest, error *e)
{
  err_t_panic (nsdbrc_read_cursors (dest, e), e);
  nsdbrc_ba_memcpy (dest);
  return SUCCESS;
}

err_t
nsdbrc_close (struct nsdb *n, struct nsdb_read_cursor *rc, error *e)
{
  for (u32 i = 0; i < rc->variables.len; ++i)
    {
      rptc_cleanup (&rc->variables.cursors[i]->rc, e);
    }

  chunk_alloc_free_all (&rc->alloc);

  slab_alloc_free (&n->dcursor_slaba, rc);

  return SUCCESS;
}
