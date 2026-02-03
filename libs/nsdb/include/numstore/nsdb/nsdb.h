#pragma once

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/slab_alloc.h>
#include <numstore/core/stride.h>
#include <numstore/intf/types.h>

struct nsdb
{
  struct pager *p;
  struct lockt *lt;
  struct slab_alloc slaba;
  struct chunk_alloc chunka;
};

struct nsdb_io
{
  void *dest;
  u32 dlen;
  u32 dcap;

  const void *src;
  u32 slen;
  u32 scap;
};

struct nsdb *nsdb_open (const char *fname, const char *recovery_fname, error *e);
err_t nsdb_close (struct nsdb *n, error *e);

struct txn *nsdb_begin_txn (struct nsdb *n, error *e);
err_t nsdb_commit (struct nsdb *n, struct txn *tx, error *e);

err_t nsdb_run (struct nsdb *n, struct txn *tx, const char *stmnt, struct nsdb_io *io, error *e);
