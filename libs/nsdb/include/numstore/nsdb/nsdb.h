#pragma once

#include <numstore/core/error.h>
#include <numstore/core/stride.h>
#include <numstore/intf/types.h>

typedef struct nsdb_s nsdb;

struct nsdb_io
{
  void *dest;
  u32 dlen;
  u32 dcap;

  const void *src;
  u32 slen;
  u32 scap;
};

nsdb *nsdb_open (const char *fname, const char *recovery_fname, error *e);
err_t nsdb_close (nsdb *n, error *e);

struct txn *nsdb_begin_txn (nsdb *n, error *e);
err_t nsdb_commit (nsdb *n, struct txn *tx, error *e);

err_t nsdb_run (nsdb *n, struct txn *tx, const char *stmnt, struct nsdb_io *io, error *e);
