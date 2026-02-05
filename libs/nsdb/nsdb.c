#include "numstore/core/error.h"
#include <numstore/nsdb/nsdb.h>

struct nsdb *
nsdb_open (const char *fname, const char *recovery_fname, error *e)
{
  return NULL;
}

err_t
nsdb_close (struct nsdb *n, error *e)
{
  return SUCCESS;
}

struct txn *
nsdb_begin_txn (struct nsdb *n, error *e)
{
  return NULL;
}

err_t
nsdb_commit (struct nsdb *n, struct txn *tx, error *e)
{
  return SUCCESS;
}

err_t
nsdb_run (struct nsdb *n, struct txn *tx, const char *stmnt, struct nsdb_io *io, error *e)
{
  return SUCCESS;
}
