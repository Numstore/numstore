#include "numstore/core/cbuffer.h"
#include "numstore/core/chunk_alloc.h"
#include "numstore/core/error.h"
#include "numstore/nsdb/multi_ba_bank.h"
#include "numstore/nsdb/multi_var_bank.h"
#include "numstore/nsdb/var_bank.h"
#include "numstore/types/byte_accessor.h"
#include "numstore/types/type_accessor.h"
#include <numstore/nsdb/nsdb_read_cursor.h>

err_t
nsdbrc_open (
    struct nsdb *n,
    struct nsdb_read_cursor *dest,
    struct read_stmt *stmt,
    error *e)
{
  err_t_panic (mvar_bank_open_all (n, &dest->variables, stmt->vrefs, stmt->gstride, e), e);
  err_t_panic (mba_bank_init (n, &dest->accessors, stmt->acc, &stmt->vrefs, &dest->variables, e), e);

  t_size size = mba_bank_byte_size (&dest->accessors);

  dest->backing = chunk_malloc (&n->chunka, 1, size, e);
  if (dest->backing == NULL)
    {
      return e->cause_code;
    }

  dest->dest = cbuffer_create (dest->backing, size);

  return SUCCESS;
}

HEADER_FUNC err_t
nsdbrc_read_cursors (struct nsdb_read_cursor *dest, error *e)
{
  for (u32 i = 0; i < dest->variables.vlen; ++i)
    {
      struct rptree_cursor *c = dest->variables.vars[i].cursor;

      switch (c->state)
        {
        case RPTS_UNSEEKED:
          {
          }
        case RPTS_SEEKING:
          {
          }
        case RPTS_DL_READING:
          {
          }
        case RPTS_SEEKED:
          {
          }
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

err_t
nsdbrc_execute (struct nsdb_read_cursor *dest, error *e)
{
  UNREACHABLE ();
}

err_t nsdbrc_close (struct nsdb_read_cursor *dest, error *e);
