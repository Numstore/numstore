#include "numstore/core/assert.h"
#include "numstore/core/error.h"
#include <numstore/nsdb/nsdb_cursor.h>

err_t
nsdbc_open (
    struct nsdb_cursor *dest,
    struct slab_alloc *rpt_alloc,
    struct read_stmt *stmt,
    error *e)
{
  return SUCCESS;
}

static err_t
nsdbc_read_cursors (struct nsdb_cursor *dest, error *e)
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
nsdbc_execute (struct nsdb_cursor *dest, error *e)
{
  UNREACHABLE ();
}

err_t nsdbc_close (struct nsdb_cursor *dest, error *e);
