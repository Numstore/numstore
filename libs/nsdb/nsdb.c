#include "numstore/compiler/compiler.h"
#include "numstore/core/assert.h"
#include "numstore/core/chunk_alloc.h"
#include "numstore/core/error.h"
#include "numstore/pager.h"
#include "numstore/rptree/oneoff.h"
#include "numstore/rs/result_set.h"
#include "numstore/types/statement.h"
#include <numstore/nsdb.h>

struct nsdb *
nsdb_open (const char *fname, const char *recovery_fname, error *e)
{
  // Allocate memory
  struct nsdb *ret = i_malloc (1, sizeof *ret, e);
  if (ret == NULL)
    {
      goto failed;
    }

  // Initialize lock table
  if (lockt_init (&ret->lt, e))
    {
      i_free (ret);
      goto failed;
    }

  // Initialize thread pool
  ret->tp = tp_open (e);
  if (ret->tp == NULL)
    {
      lockt_destroy (&ret->lt);
      i_free (ret);
      goto failed;
    }

  // Create a new pager
  ret->p = pgr_open (fname, recovery_fname, &ret->lt, ret->tp, e);
  if (ret->p == NULL)
    {
      lockt_destroy (&ret->lt);
      i_free (ret);
      tp_free (ret->tp, e);
      goto failed;
    }

  if (nsdb_rp_init (&ret->rp, ret->p, &ret->lt, e))
    {
      lockt_destroy (&ret->lt);
      i_free (ret);
      tp_free (ret->tp, e);
      pgr_close (ret->p, e);
      goto failed;
    }

  if (pgr_get_npages (ret->p) == 1)
    {
      // Create a variable hash table page
      bool before = e->print_msg_on_error;
      e->print_msg_on_error = false;
      if (varh_init_hash_page (ret->p, e) < 0)
        {
          e->print_msg_on_error = before;
          lockt_destroy (&ret->lt);
          i_free (ret);
          tp_free (ret->tp, e);
          nsdb_rp_free (&ret->rp);
          pgr_close (ret->p, e);
          goto failed;
        }
      e->print_msg_on_error = before;
    }

  return ret;

failed:
  return NULL;
}

err_t
nsdb_close (struct nsdb *n, error *e)
{
  pgr_close (n->p, e);
  tp_free (n->tp, e);
  lockt_destroy (&n->lt);
  nsdb_rp_free (&n->rp);

  if (e->cause_code < 0)
    {
      return e->cause_code;
    }

  return SUCCESS;
}

struct txn *
nsdb_begin_txn (struct nsdb *n, error *e)
{
  struct txn *tx = i_malloc (1, sizeof *tx, e);
  if (tx == NULL)
    {
      return NULL;
    }

  if (pgr_begin_txn (tx, n->p, e))
    {
      return NULL;
    }

  return tx;
}

err_t
nsdb_commit (struct nsdb *n, struct txn *tx, error *e)
{
  return pgr_commit (n->p, tx, e);
}

static inline int
nsdb_auto_begin_txn (struct nsdb *n, struct txn **tx, struct txn *auto_txn, error *e)
{
  int auto_txn_started = 0;

  if (*tx == NULL)
    {
      if (pgr_begin_txn (auto_txn, n->p, e))
        {
          return e->cause_code;
        }
      auto_txn_started = true;
      *tx = auto_txn;
    }

  return auto_txn_started;
}

static inline err_t
nsdb_auto_commit (struct nsdb *n, struct txn *tx, int auto_txn_started, error *e)
{
  if (auto_txn_started)
    {
      err_t_wrap (pgr_commit (n->p, tx, e), e);
    }
  return SUCCESS;
}

static inline err_t
validate_nsdb_io (const struct statement *stmt, struct nsdb_io *io, error *e)
{
  switch (stmt->type)
    {
    case ST_CREATE:
    case ST_DELETE:
    case ST_REMOVE:
      {
        return SUCCESS;
      }
    case ST_INSERT:
      {
        if (io == NULL || io->slen == 0)
          {
            return error_causef (e, ERR_INVALID_ARGUMENT, "Must provide an input output interface for insert");
          }
        if (io->scap < io->slen)
          {
            return error_causef (e, ERR_INVALID_ARGUMENT, "Source capacity must be greater than source length");
          }
        return SUCCESS;
      }
    case ST_READ:
      {
        if (io == NULL || io->dlen == 0)
          {
            return error_causef (e, ERR_INVALID_ARGUMENT, "Must provide an input output interface for read");
          }
        if (io->dcap < io->dlen)
          {
            return error_causef (e, ERR_INVALID_ARGUMENT, "Dest capacity must be greater than dest length");
          }
        return SUCCESS;
      }
    }

  UNREACHABLE ();
}

err_t
nsdb_execute (struct nsdb *n, struct txn *tx, const char *stmnt, struct nsdb_io *io, error *e)
{
  struct statement stmt;

  struct chunk_alloc query_space;
  chunk_alloc_create_default (&query_space);

  if (compile_statement (&stmt, stmnt, &query_space, e))
    {
      return e->cause_code;
    }

  // Validate input
  if (validate_nsdb_io (&stmt, io, e))
    {
      goto theend_norollback;
    }

  // BEGIN TXN
  struct txn auto_txn;
  int auto_txn_started = false;
  if (stmt_requires_txn (stmt.type))
    {
      auto_txn_started = nsdb_auto_begin_txn (n, &tx, &auto_txn, e);
      if (auto_txn_started < 0)
        {
          goto theend_norollback;
        }
    }

  switch (stmt.type)
    {
    case ST_CREATE:
      {
        if (nsdb_rp_create (&n->rp, tx, stmt.create.vname, &stmt.create.vtype, e))
          {
            goto theend_rollback;
          }
        break;
      }
    case ST_DELETE:
      {
        if (nsdb_rp_delete (&n->rp, tx, stmt.create.vname, e))
          {
            goto theend_rollback;
          }
        break;
      }
    case ST_INSERT:
      {
        const struct variable *var = nsdb_rp_get_variable (&n->rp, stmt.insert.vname, e);
        if (var == NULL)
          {
            return e->cause_code;
          }

        struct rptree_cursor *rc = nsdb_rp_open_cursor (&n->rp, stmt.insert.vname, e);
        if (rc == NULL)
          {
            nsdb_rp_free_variable (&n->rp, var, e);
            return e->cause_code;
          }

        // DO INSERT
        {
          rptc_enter_transaction (rc, tx);
          t_size size = type_byte_size (var->dtype);
          if (rptof_insert (rc, io->src, size * stmt.insert.ofst, size, stmt.insert.nelems, e))
            {
              goto theend_rollback;
            }
          rptc_leave_transaction (rc);
        }

        // UPDATE VARIABLE META
        {
          if (nsdb_rp_update (&n->rp, tx, var, rc->root, rc->total_size, e))
            {
              goto theend_rollback;
            }
        }

        // CLOSE RPTC
        {
          if (nsdb_rp_close_cursor (&n->rp, rc, e))
            {
              goto theend_rollback;
            }
          rc = NULL;
          if (nsdb_rp_free_variable (&n->rp, var, e))
            {
              goto theend_rollback;
            }
          var = NULL;
        }

        break;
      }
    case ST_READ:
      {
        // Fetch the variable from the db
        struct result_set *rs = nsdb_rp_get_result_set (&n->rp, &stmt.read.tr, &query_space, stmt.read.str, e);

        while (true)
          {
            rs_execute (rs, e);
          }

        // CLOSE RPTC
        {
          // TODO
        }

        break;
      }
    case ST_REMOVE:
      {
        panic ("TODO");
      }
    }

  if (stmt_requires_txn (stmt.type))
    {
      if (auto_txn_started)
        {
          if (pgr_commit (n->p, tx, e))
            {
              goto theend_rollback;
            }
        }
    }

  return SUCCESS;

theend_rollback:
  if (e->cause_code && stmt_requires_txn (stmt.type))
    {
      pgr_rollback (n->p, tx, 0, e);
    }

theend_norollback:
  return e->cause_code;
}
