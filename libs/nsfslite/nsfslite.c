/*
 * Copyright 2025 Theo Lincke
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License->
 * You may obtain a copy of the License at
 *
 *     http://www.apache->org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License->
 *
 * Description:
 *   Implements nsfslite->h. Core implementation of NumStore File System Lite, providing
 *   simplified database operations for managing named variables with transaction support.
 *   Handles cursor management, pager integration, and implements all CRUD operations
 *   with automatic or explicit transaction handling.
 */

#include <nsfslite.h>
#include <numstore/compiler/compiler.h>
#include <numstore/core/string.h>
#include <numstore/core/threadpool.h>
#include <numstore/repository/nsdb_rp.h>
#include <numstore/types/types.h>

#include <numstore/compiler/lexer.h>
#include <numstore/compiler/parser/stride.h>
#include <numstore/compiler/parser/type.h>
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
#include <numstore/var/attr.h>
#include <numstore/var/var_cursor.h>

#include <fcntl.h>
#include <pthread.h>

struct nsfslite_s
{
  struct pager *p;
  struct lockt lt;
  struct thread_pool *tp;
  struct nsdb_rp rp;
};

DEFINE_DBG_ASSERT (
    struct nsfslite_s, nsfslite, n, {
      ASSERT (n);
      ASSERT (n->p);
    })

static inline int
nsfslite_auto_begin_txn (nsfslite *n, struct txn **tx, struct txn *auto_txn, error *e)
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
nsfslite_auto_commit (nsfslite *n, struct txn *tx, int auto_txn_started, error *e)
{
  if (auto_txn_started)
    {
      err_t_wrap (pgr_commit (n->p, tx, e), e);
    }
  return SUCCESS;
}

nsfslite *
nsfslite_open (const char *fname, const char *recovery_fname, error *e)
{
  i_log_info ("nsfslite_open: fname=%s recovery=%s\n",
              fname, recovery_fname ? recovery_fname : "none");

  // Allocate memory
  nsfslite *ret = i_malloc (1, sizeof *ret, e);
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
nsfslite_close (nsfslite *n, error *e)
{
  DBG_ASSERT (nsfslite, n);

  pgr_close (n->p, e);
  tp_free (n->tp, e);
  lockt_destroy (&n->lt);
  nsdb_rp_free (&n->rp);

  if (e->cause_code < 0)
    {
      i_log_error ("nsfslite_close failed: code=%d\n", e->cause_code);
      return e->cause_code;
    }

  i_log_debug ("nsfslite_close: success\n");
  return SUCCESS;
}

err_t
nsfslite_new (nsfslite *n, struct txn *tx, const char *name, const char *type, error *e)
{
  DBG_ASSERT (nsfslite, n);

  i_log_info ("nsfslite_new: name=%s\n", name);

  // INIT
  struct chunk_alloc temp;
  chunk_alloc_create_default (&temp);

  struct type t;

  // PARSE TYPE STRING
  if (compile_type (&t, type, &temp, e))
    {
      goto theend;
    }

  // MAYBE BEGIN TXN
  struct txn auto_txn;
  int auto_txn_started = nsfslite_auto_begin_txn (n, &tx, &auto_txn, e);
  if (auto_txn_started < 0)
    {
      goto theend;
    }

  // CREATE A NEW VARIABLE
  if (nsdb_rp_create (&n->rp, tx, strfcstr (name), &t, e))
    {
      goto theend;
    }

  // COMMIT
  if (auto_txn_started)
    {
      if (pgr_commit (n->p, tx, e))
        {
          goto theend;
        }
    }

theend:
  chunk_alloc_free_all (&temp);

  return e->cause_code;
}

err_t
nsfslite_delete (nsfslite *n, struct txn *tx, const char *name, error *e)
{
  DBG_ASSERT (nsfslite, n);

  // MAYBE BEGIN AUTO TXN
  struct txn auto_txn;
  int auto_txn_started = nsfslite_auto_begin_txn (n, &tx, &auto_txn, e);
  if (auto_txn_started < 0)
    {
      goto theend;
    }

  // DELETE VARIABLE
  if (nsdb_rp_delete (&n->rp, tx, strfcstr (name), e))
    {
      goto theend;
    }

  // TODO - delete the rptree

  // COMMIT
  if (nsfslite_auto_commit (n, tx, auto_txn_started, e))
    {
      goto theend;
    }

theend:

  if (e->cause_code)
    {
      pgr_rollback (n->p, tx, 0, e);
    }

  return e->cause_code;
}

sb_size
nsfslite_fsize (nsfslite *n, const char *name, error *e)
{
  DBG_ASSERT (nsfslite, n);

  const struct variable *var = nsdb_rp_get_variable (&n->rp, strfcstr (name), e);

  if (var == NULL)
    {
      return e->cause_code;
    }

  pgno ret = var->var_root;
  nsdb_rp_free_variable (&n->rp, var, e);

  if (e->cause_code)
    {
      return e->cause_code;
    }
  else
    {
      return ret;
    }
}

struct txn *
nsfslite_begin_txn (nsfslite *n, error *e)
{
  struct txn *tx = i_malloc (1, sizeof *tx, e);
  if (tx == NULL)
    {
      error_log_consume (e);
      return NULL;
    }

  if (pgr_begin_txn (tx, n->p, e))
    {
      error_log_consume (e);
      return NULL;
    }

  return tx;
}

err_t
nsfslite_commit (nsfslite *n, struct txn *tx, error *e)
{
  int ret = pgr_commit (n->p, tx, e);
  return ret;
}

err_t
nsfslite_insert (
    nsfslite *n,
    const char *name,
    struct txn *tx,
    const void *src,
    b_size ofst,
    b_size nelem,
    error *e)
{
  const struct variable *var = nsdb_rp_get_variable (&n->rp, strfcstr (name), e);
  if (var == NULL)
    {
      return e->cause_code;
    }

  struct rptree_cursor *rc = nsdb_rp_open_cursor (&n->rp, strfcstr (name), e);
  if (rc == NULL)
    {
      nsdb_rp_free_variable (&n->rp, var, e);
      return e->cause_code;
    }

  // MAYBE BEGIN TXN
  struct txn auto_txn;
  int auto_txn_started = nsfslite_auto_begin_txn (n, &tx, &auto_txn, e);
  if (auto_txn_started < 0)
    {
      goto theend;
    }

  // DO INSERT
  {
    rptc_enter_transaction (rc, tx);
    t_size size = type_byte_size (var->dtype);
    if (rptof_insert (rc, src, size * ofst, size, nelem, e))
      {
        goto theend;
      }
    rptc_leave_transaction (rc);
  }

  // UPDATE VARIABLE META
  {
    if (nsdb_rp_update (&n->rp, tx, var, rc->root, rc->total_size, e))
      {
        goto theend;
      }
  }

  // CLOSE RPTC
  {
    if (nsdb_rp_close_cursor (&n->rp, rc, e))
      {
        goto theend;
      }
    rc = NULL;
    if (nsdb_rp_free_variable (&n->rp, var, e))
      {
        goto theend;
      }
    var = NULL;
  }

  // COMMIT
  if (nsfslite_auto_commit (n, tx, auto_txn_started, e))
    {
      goto theend;
    }

theend:
  if (rc != NULL)
    {
      nsdb_rp_close_cursor (&n->rp, rc, e);
    }
  if (var != NULL)
    {
      nsdb_rp_free_variable (&n->rp, var, e);
    }

  if (e->cause_code)
    {
      pgr_rollback (n->p, tx, 0, e);
      return e->cause_code;
    }
  else
    {
      return SUCCESS;
    }
}

err_t
nsfslite_write (
    nsfslite *n,
    const char *name,
    struct txn *tx,
    const void *src,
    const char *stride,
    error *e)
{
  struct stride _stride;
  struct user_stride ustride;

  // COMPILE STRIDE
  if (compile_stride (&ustride, stride, e))
    {
      goto theend;
    }

  const struct variable *var = nsdb_rp_get_variable (&n->rp, strfcstr (name), e);
  if (var == NULL)
    {
      return e->cause_code;
    }

  struct rptree_cursor *rc = nsdb_rp_open_cursor (&n->rp, strfcstr (name), e);
  if (rc == NULL)
    {
      nsdb_rp_free_variable (&n->rp, var, e);
      return e->cause_code;
    }

  t_size size = type_byte_size (var->dtype);
  if (stride_resolve (&_stride, ustride, rc->total_size / size, e))
    {
      goto theend;
    }

  // MAYBE BEGIN TXN
  struct txn auto_txn;
  int auto_txn_started = nsfslite_auto_begin_txn (n, &tx, &auto_txn, e);
  if (auto_txn_started < 0)
    {
      goto theend;
    }

  // DO WRITE
  {
    rptc_enter_transaction (rc, tx);

    if (rptof_write (rc, src, size, size * _stride.start, _stride.stride, _stride.nelems, e))
      {
        goto theend;
      }
    rptc_leave_transaction (rc);
  }

  // CLOSE RPTC
  {
    if (nsdb_rp_close_cursor (&n->rp, rc, e))
      {
        goto theend;
      }
    rc = NULL;
    if (nsdb_rp_free_variable (&n->rp, var, e))
      {
        goto theend;
      }
    var = NULL;
  }

  // COMMIT
  if (nsfslite_auto_commit (n, tx, auto_txn_started, e))
    {
      goto theend;
    }

theend:
  if (rc != NULL)
    {
      nsdb_rp_close_cursor (&n->rp, rc, e);
    }
  if (var != NULL)
    {
      nsdb_rp_free_variable (&n->rp, var, e);
    }

  if (e->cause_code)
    {
      pgr_rollback (n->p, tx, 0, e);
      return e->cause_code;
    }
  else
    {
      return SUCCESS;
    }
}

sb_size
nsfslite_read (
    nsfslite *n,
    const char *name,
    void *dest,
    const char *stride,
    error *e)
{
  struct stride _stride;
  struct user_stride ustride;

  // COMPILE STRIDE
  if (compile_stride (&ustride, stride, e))
    {
      goto theend;
    }

  // Fetch the variable from the db
  const struct variable *var = nsdb_rp_get_variable (&n->rp, strfcstr (name), e);
  if (var == NULL)
    {
      return e->cause_code;
    }

  // Open the rptree cursor on that variable
  struct rptree_cursor *rc = nsdb_rp_open_cursor (&n->rp, strfcstr (name), e);
  if (rc == NULL)
    {
      nsdb_rp_free_variable (&n->rp, var, e);
      return e->cause_code;
    }

  // Resolve stride for this variable
  t_size size = type_byte_size (var->dtype);
  if (rc->total_size % size != 0)
    {
      return error_causef (
          e, ERR_CORRUPT,
          "Type with size: %" PRt_size " has %" PRb_size " bytes, "
          "which is not a multiple of it's type size",
          size, rc->total_size);
    }
  if (stride_resolve (&_stride, ustride, rc->total_size / size, e))
    {
      goto theend;
    }

  // Do Read
  sb_size ret = rptof_read (
      rc,
      dest,
      size,
      size * _stride.start,
      _stride.stride,
      _stride.nelems,
      e);
  if (ret < 0)
    {
      nsdb_rp_free_variable (&n->rp, var, e);
      nsdb_rp_close_cursor (&n->rp, rc, e);
      goto theend;
    }

  // CLOSE RPTC
  {
    if (nsdb_rp_close_cursor (&n->rp, rc, e))
      {
        goto theend;
      }
    rc = NULL;
    if (nsdb_rp_free_variable (&n->rp, var, e))
      {
        goto theend;
      }
    var = NULL;
  }

theend:
  if (rc != NULL)
    {
      nsdb_rp_close_cursor (&n->rp, rc, e);
    }
  if (var != NULL)
    {
      nsdb_rp_free_variable (&n->rp, var, e);
    }

  if (e->cause_code)
    {
      return e->cause_code;
    }
  else
    {
      return ret;
    }
}

err_t
nsfslite_remove (
    nsfslite *n,
    const char *name,
    struct txn *tx,
    void *dest,
    const char *stride,
    error *e)
{
  struct stride _stride;
  struct user_stride ustride;

  // COMPILE STRIDE
  if (compile_stride (&ustride, stride, e))
    {
      goto theend;
    }

  const struct variable *var = nsdb_rp_get_variable (&n->rp, strfcstr (name), e);
  if (var == NULL)
    {
      return e->cause_code;
    }

  struct rptree_cursor *rc = nsdb_rp_open_cursor (&n->rp, strfcstr (name), e);
  if (rc == NULL)
    {
      nsdb_rp_free_variable (&n->rp, var, e);
      return e->cause_code;
    }

  t_size size = type_byte_size (var->dtype);
  if (stride_resolve (&_stride, ustride, rc->total_size / size, e))
    {
      goto theend;
    }

  // MAYBE BEGIN TXN
  struct txn auto_txn;
  int auto_txn_started = nsfslite_auto_begin_txn (n, &tx, &auto_txn, e);
  if (auto_txn_started < 0)
    {
      goto theend;
    }

  // DO REMOVE
  {
    rptc_enter_transaction (rc, tx);

    if (rptof_remove (rc, dest, size, size * _stride.start, _stride.stride, _stride.nelems, e))
      {
        goto theend;
      }
    rptc_leave_transaction (rc);
  }

  // CLOSE RPTC
  {
    if (nsdb_rp_close_cursor (&n->rp, rc, e))
      {
        goto theend;
      }
    rc = NULL;
    if (nsdb_rp_free_variable (&n->rp, var, e))
      {
        goto theend;
      }
    var = NULL;
  }

  // COMMIT
  if (nsfslite_auto_commit (n, tx, auto_txn_started, e))
    {
      goto theend;
    }

theend:
  if (rc != NULL)
    {
      nsdb_rp_close_cursor (&n->rp, rc, e);
    }
  if (var != NULL)
    {
      nsdb_rp_free_variable (&n->rp, var, e);
    }

  if (e->cause_code)
    {
      pgr_rollback (n->p, tx, 0, e);
      return e->cause_code;
    }
  else
    {
      return SUCCESS;
    }
}
