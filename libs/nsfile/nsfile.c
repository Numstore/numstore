#include <nsfile.h>

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/slab_alloc.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/pager.h>
#include <numstore/pager/lock_table.h>
#include <numstore/rptree/_rebalance.h>
#include <numstore/rptree/oneoff.h>
#include <numstore/rptree/rptree_cursor.h>

struct nsfile_s
{
  struct pager *p;
  struct lockt lt;
  struct thread_pool *tp;
  struct latch l;
};

struct txn_s
{
  struct txn tx;
};

static inline int
nsfile_auto_begin_txn (nsfile *n, txn **tx, txn *auto_txn, error *e)
{
  int auto_txn_started = 0;

  if (*tx == NULL)
    {
      if (pgr_begin_txn (&auto_txn->tx, n->p, e))
        {
          return e->cause_code;
        }
      auto_txn_started = true;
      *tx = auto_txn;
    }

  return auto_txn_started;
}

static inline err_t
nsfile_auto_commit (nsfile *n, txn *tx, int auto_txn_started, error *e)
{
  if (auto_txn_started)
    {
      err_t_wrap (pgr_commit (n->p, &tx->tx, e), e);
    }
  return SUCCESS;
}

nsfile *
nsfile_open (const char *fname, const char *recovery)
{
  error e = error_create ();

  // Allocate memory
  nsfile *ret = i_malloc (1, sizeof *ret, &e);
  if (ret == NULL)
    {
      goto failed;
    }

  // Initialize lock table
  if (lockt_init (&ret->lt, &e))
    {
      i_free (ret);
      goto failed;
    }

  // Initialize thread pool
  ret->tp = tp_open (&e);
  if (ret->tp == NULL)
    {
      lockt_destroy (&ret->lt);
      i_free (ret);
      goto failed;
    }

  // Create a new pager
  ret->p = pgr_open (fname, recovery, &ret->lt, ret->tp, &e);
  if (ret->p == NULL)
    {
      tp_free (ret->tp, &e);
      lockt_destroy (&ret->lt);
      i_free (ret);
      goto failed;
    }

  return ret;

failed:
  return NULL;
}

int
nsfile_close (nsfile *n)
{
  error e = error_create ();
  pgr_close (n->p, &e);
  tp_free (n->tp, &e);
  lockt_destroy (&n->lt);

  if (e.cause_code < 0)
    {
      return e.cause_code;
    }

  return 0;
}

txn *
nsfile_begin_txn (nsfile *fd)
{
  error e = error_create ();
  struct txn_s *tx = i_malloc (1, sizeof *tx, &e);
  if (tx == NULL)
    {
      return NULL;
    }

  if (pgr_begin_txn (&tx->tx, fd->p, &e))
    {
      i_free (tx);
      return NULL;
    }

  return tx;
}

int
nsfile_commit (nsfile *fd, txn *t)
{
  error e = error_create ();
  if (pgr_commit (fd->p, &t->tx, &e))
    {
      return -1;
    }

  i_free (t);

  return 0;
}

#define MAYBE_BEGIN_TXN(n, tx, e)                                      \
  txn auto_txn;                                                        \
  int auto_txn_started = nsfile_auto_begin_txn (n, &tx, &auto_txn, e); \
  if (auto_txn_started < 0)                                            \
    {                                                                  \
      goto failed;                                                     \
    }

#define MAYBE_COMMIT(n, tx, e)             \
  if (auto_txn_started)                    \
    {                                      \
      if (pgr_commit ((n)->p, &tx->tx, e)) \
        {                                  \
          goto failed;                     \
        }                                  \
    }

int
nsfile_insert (nsfile *fd, txn *tx, const void *buf, size_t n, __off_t offset)
{
  struct rptree_cursor rc;
  error e = error_create ();

  MAYBE_BEGIN_TXN (fd, tx, &e);

  if (pgr_get_npages (fd->p))
    {
      rptc_new (&rc, &tx->tx, fd->p, &fd->lt);
    }
  else
    {
      if (rptc_open (&rc, 1, fd->p, &fd->lt, &e))
        {
          goto failed;
        }
    }

  if (rptof_insert (&rc, buf, offset, 1, n, &e))
    {
      goto failed;
    }

  if (rptc_cleanup (&rc, &e))
    {
      goto failed;
    }

  MAYBE_COMMIT (fd, tx, &e);

  return 0;

failed:
  if (pgr_rollback (fd->p, &tx->tx, 0, &e))
    {
      panic ("DB is in an unrecoverable state");
    }
  return -1;
}

int
nsfile_write (nsfile *fd, txn *tx, int loc, const void *buf, size_t n, __off_t offset)
{
  struct rptree_cursor rc;
  error e = error_create ();

  MAYBE_BEGIN_TXN (fd, tx, &e);

  if (pgr_get_npages (fd->p))
    {
      rptc_new (&rc, &tx->tx, fd->p, &fd->lt);
    }
  else
    {
      if (rptc_open (&rc, 1, fd->p, &fd->lt, &e))
        {
          goto failed;
        }
    }

  if (rptof_write (&rc, buf, 1, offset, 1, n, &e))
    {
      goto failed;
    }

  if (rptc_cleanup (&rc, &e))
    {
      goto failed;
    }

  MAYBE_COMMIT (fd, tx, &e);

  return 0;

failed:
  if (pgr_rollback (fd->p, &tx->tx, 0, &e))
    {
      panic ("DB is in an unrecoverable state");
    }
  return -1;
}

size_t
nsfile_read (nsfile *fd, void *dest, size_t n, __off_t offset)
{

  if (pgr_get_npages (fd->p))
    {
      return 0;
    }

  struct rptree_cursor rc;
  error e = error_create ();

  if (rptc_open (&rc, 1, fd->p, &fd->lt, &e))
    {
      goto failed;
    }

  if (rptof_read (&rc, dest, 1, offset, 1, n, &e))
    {
      goto failed;
    }

  if (rptc_cleanup (&rc, &e))
    {
      goto failed;
    }

  return 0;

failed:
  return -1;
}

size_t
nsfile_remove (nsfile *fd, txn *tx, size_t ofst, void *dest, size_t n, __off_t offset)
{
  struct rptree_cursor rc;
  error e = error_create ();

  MAYBE_BEGIN_TXN (fd, tx, &e);

  if (pgr_get_npages (fd->p))
    {
      rptc_new (&rc, &tx->tx, fd->p, &fd->lt);
    }
  else
    {
      if (rptc_open (&rc, 1, fd->p, &fd->lt, &e))
        {
          goto failed;
        }
    }

  if (rptof_remove (&rc, dest, 1, offset, 1, n, &e))
    {
      goto failed;
    }

  if (rptc_cleanup (&rc, &e))
    {
      goto failed;
    }

  MAYBE_COMMIT (fd, tx, &e);

  return 0;

failed:
  if (pgr_rollback (fd->p, &tx->tx, 0, &e))
    {
      panic ("DB is in an unrecoverable state");
    }
  return -1;
}
