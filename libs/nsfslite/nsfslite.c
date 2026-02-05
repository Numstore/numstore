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

#include "numstore/compiler/compiler.h"
#include "numstore/types/types.h"
#include <fcntl.h>
#include <nsfslite.h>

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

#include <pthread.h>

union cursor
{
  struct rptree_cursor rptc;
  struct var_cursor vpc;
};

struct nsfslite_s
{
  struct pager *p;
  struct slab_alloc alloc;
  struct lockt lt;
  struct thread_pool *tp;
  struct latch l;
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
      tp_free (ret->tp, e);
      lockt_destroy (&ret->lt);
      i_free (ret);
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
          pgr_close (ret->p, e);
          i_free (ret);
          goto failed;
        }
      e->print_msg_on_error = before;
    }

  slab_alloc_init (&ret->alloc, sizeof (union cursor), 512);

  return ret;

failed:
  return NULL;
}

err_t
nsfslite_close (nsfslite *n, error *e)
{
  DBG_ASSERT (nsfslite, n);

  pgr_close (n->p, e);
  slab_alloc_destroy (&n->alloc);
  tp_free (n->tp, e);
  lockt_destroy (&n->lt);

  if (e->cause_code < 0)
    {
      i_log_error ("nsfslite_close failed: code=%d\n", e->cause_code);
      return e->cause_code;
    }

  i_log_debug ("nsfslite_close: success\n");
  return SUCCESS;
}

spgno
nsfslite_new (nsfslite *n, struct txn *tx, const char *name, const char *type, error *e)
{
  DBG_ASSERT (nsfslite, n);

  i_log_info ("nsfslite_new: name=%s\n", name);

  spgno ret = -1;

  // INIT
  union cursor *vc = slab_alloc_alloc (&n->alloc, e);
  if (vc == NULL)
    {
      return e->cause_code;
    }

  struct chunk_alloc temp;
  chunk_alloc_create_default (&temp);

  if (varc_initialize (&vc->vpc, n->p, e) < 0)
    {
      goto theend;
    }

  struct var_create_params src = {
    .vname = strfcstr (name),
  };

  // PARSE TYPE STRING
  if (compile_type (&src.t, type, &temp, e))
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

  // CREATE A NEW VARIABLE PAGE
  {
    varc_enter_transaction (&vc->vpc, tx);

    ret = vpc_new (&vc->vpc, src, e);
    if (ret < 0)
      {
        goto theend;
      }

    varc_leave_transaction (&vc->vpc);
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
  slab_alloc_free (&n->alloc, vc);
  chunk_alloc_free_all (&temp);

  if (e->cause_code)
    {
      return e->cause_code;
    }
  else
    {
      return ret;
    }
}

spgno
nsfslite_get_id (nsfslite *n, const char *name, error *e)
{
  DBG_ASSERT (nsfslite, n);

  spgno ret = -1;

  // INIT
  union cursor *c = slab_alloc_alloc (&n->alloc, e);
  if (c == NULL)
    {
      return e->cause_code;
    }

  err_t_wrap_goto (varc_initialize (&c->vpc, n->p, e), theend, e);

  // GET VARIABLE
  struct var_get_params params = {
    .vname = strfcstr (name),
  };

  ret = vpc_get (&c->vpc, NULL, &params, e);
  if (ret < 0)
    {
      goto theend;
    }

theend:
  slab_alloc_free (&n->alloc, c);

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
nsfslite_delete (nsfslite *n, struct txn *tx, const char *name, error *e)
{
  DBG_ASSERT (nsfslite, n);

  // INIT
  union cursor *c = slab_alloc_alloc (&n->alloc, e);

  if (c == NULL)
    {
      return e->cause_code;
    }

  varc_initialize (&c->vpc, n->p, e);

  // MAYBE BEGIN AUTO TXN
  struct txn auto_txn;
  int auto_txn_started = nsfslite_auto_begin_txn (n, &tx, &auto_txn, e);
  if (auto_txn_started < 0)
    {
      goto theend;
    }

  // DELETE VARIABLE
  {
    varc_enter_transaction (&c->vpc, tx);

    if (vpc_delete (&c->vpc, strfcstr (name), e))
      {
        goto theend;
      }

    varc_leave_transaction (&c->vpc);
  }

  // TODO - delete the rptree

  // COMMIT
  if (nsfslite_auto_commit (n, tx, auto_txn_started, e))
    {
      goto theend;
    }

theend:
  slab_alloc_free (&n->alloc, &c->vpc);
  return e->cause_code;
}

sb_size
nsfslite_fsize (nsfslite *n, pgno id, error *e)
{
  DBG_ASSERT (nsfslite, n);

  union cursor *vc = slab_alloc_alloc (&n->alloc, e);
  if (vc == NULL)
    {
      return e->cause_code;
    }

  struct var_get_by_id_params params = {
    .id = id,
  };

  if (varc_initialize (&vc->vpc, n->p, e))
    {
      goto theend;
    }

  // FETCH THE VARIABLE
  if (vpc_get_by_id (&vc->vpc, NULL, &params, e))
    {
      goto theend;
    }

  slab_alloc_free (&n->alloc, vc);

theend:
  slab_alloc_free (&n->alloc, vc);

  if (e->cause_code)
    {
      return e->cause_code;
    }
  else
    {
      return params.nbytes;
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

static err_t
nsfslite_get_root (
    nsfslite *n,
    union cursor **rc,
    union cursor **vc,
    struct chunk_alloc *temp,
    struct var_get_by_id_params *params,
    error *e)
{
  DBG_ASSERT (nsfslite, n);
  *rc = NULL;
  *vc = NULL;

  // ALLOCATE MEMORY
  *rc = slab_alloc_alloc (&n->alloc, e);
  if (rc == NULL)
    {
      return e->cause_code;
    }

  *vc = slab_alloc_alloc (&n->alloc, e);
  if (vc == NULL)
    {
      slab_alloc_free (&n->alloc, *rc);
      *rc = NULL;
      return e->cause_code;
    }

  if (varc_initialize (&(*vc)->vpc, n->p, e))
    {
      goto failed;
    }

  // GET VARIABLE
  if (vpc_get_by_id (&(*vc)->vpc, temp, params, e))
    {
      goto failed;
    }

  // INIT RPTREE CURSOR
  if (rptc_open (&(*rc)->rptc, params->pg0, n->p, &n->lt, e))
    {
      goto failed;
    }

  return SUCCESS;

failed:
  slab_alloc_free (&n->alloc, *rc);
  slab_alloc_free (&n->alloc, *vc);
  *rc = NULL;
  *vc = NULL;
  return e->cause_code;
}

err_t
nsfslite_insert (
    nsfslite *n,
    pgno id,
    struct txn *tx,
    const void *src,
    b_size ofst,
    b_size nelem,
    error *e)
{
  union cursor *rc;
  union cursor *vc;
  struct chunk_alloc temp;
  chunk_alloc_create_default (&temp);
  struct var_get_by_id_params params = {
    .id = id,
  };

  err_t_wrap (nsfslite_get_root (n, &rc, &vc, &temp, &params, e), e);

  // MAYBE BEGIN TXN
  struct txn auto_txn;
  int auto_txn_started = nsfslite_auto_begin_txn (n, &tx, &auto_txn, e);
  if (auto_txn_started < 0)
    {
      goto theend;
    }

  // DO INSERT
  {
    rptc_enter_transaction (&rc->rptc, tx);

    t_size size = type_byte_size (&params.t);
    if (rptof_insert (&rc->rptc, src, size * ofst, size, nelem, e))
      {
        rptc_cleanup (&rc->rptc, e);
        goto theend;
      }

    rptc_leave_transaction (&rc->rptc);

    if (rptc_cleanup (&rc->rptc, e))
      {
        goto theend;
      }
  }

  // UPDATE VARIABLE META
  {
    varc_enter_transaction (&vc->vpc, tx);

    struct var_update_by_id_params uparams = {
      .id = id,
      .root = rc->rptc.root,
      .nbytes = rc->rptc.total_size,
    };
    if (vpc_update_by_id (&vc->vpc, &uparams, e))
      {
        goto theend;
      }
  }

  // COMMIT
  if (nsfslite_auto_commit (n, tx, auto_txn_started, e))
    {
      goto theend;
    }

theend:
  slab_alloc_free (&n->alloc, rc);
  slab_alloc_free (&n->alloc, vc);

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
    pgno id,
    struct txn *tx,
    const void *src,
    const char *stride,
    error *e)
{
  union cursor *rc;
  union cursor *vc;
  struct chunk_alloc temp;
  chunk_alloc_create_default (&temp);
  struct var_get_by_id_params params = {
    .id = id,
  };
  struct stride _stride;

  // PARSE STRIDE STRING
  struct user_stride ustride;
  if (compile_stride (&ustride, stride, e))
    {
      goto theend;
    }

  if (nsfslite_get_root (n, &rc, &vc, &temp, &params, e))
    {
      goto theend;
    }

  // RESOLVE STRIDE
  t_size size = type_byte_size (&params.t);
  if (stride_resolve (&_stride, ustride, rc->rptc.total_size / size, e))
    {
      rptc_cleanup (&rc->rptc, e);
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
    rptc_enter_transaction (&rc->rptc, tx);

    if (rptof_write (
            &rc->rptc,
            src,
            size,
            size * _stride.start,
            _stride.stride,
            _stride.nelems,
            e))
      {
        rptc_cleanup (&rc->rptc, e);
        goto theend;
      }

    rptc_leave_transaction (&rc->rptc);

    if (rptc_cleanup (&rc->rptc, e))
      {
        goto theend;
      }
  }

  // COMMIT
  if (nsfslite_auto_commit (n, tx, auto_txn_started, e))
    {
      goto theend;
    }

theend:
  slab_alloc_free (&n->alloc, rc);
  slab_alloc_free (&n->alloc, vc);

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
    pgno id,
    void *dest,
    const char *stride,
    error *e)
{
  union cursor *rc;
  union cursor *vc;
  struct chunk_alloc temp;
  chunk_alloc_create_default (&temp);
  struct var_get_by_id_params params = {
    .id = id,
  };
  struct stride _stride;
  sb_size ret = -1;

  // PARSE STRIDE STRING
  struct user_stride ustride;
  if (compile_stride (&ustride, stride, e))
    {
      goto theend;
    }

  if (nsfslite_get_root (n, &rc, &vc, &temp, &params, e))
    {
      goto theend;
    }

  // RESOLVE STRIDE
  t_size size = type_byte_size (&params.t);
  if (stride_resolve (&_stride, ustride, rc->rptc.total_size / size, e))
    {
      rptc_cleanup (&rc->rptc, e);
      goto theend;
    }

  // DO READ
  {
    ret = rptof_read (
        &rc->rptc,
        dest,
        size,
        size * _stride.start,
        _stride.stride,
        _stride.nelems,
        e);
    if (ret < 0)
      {
        rptc_cleanup (&rc->rptc, e);
        goto theend;
      }

    if (rptc_cleanup (&rc->rptc, e))
      {
        goto theend;
      }
  }

theend:
  slab_alloc_free (&n->alloc, rc);
  slab_alloc_free (&n->alloc, vc);

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
    pgno id,
    struct txn *tx,
    void *dest,
    const char *stride,
    error *e)
{
  union cursor *rc;
  union cursor *vc;
  struct chunk_alloc temp;
  chunk_alloc_create_default (&temp);
  struct var_get_by_id_params params = {
    .id = id,
  };
  struct stride _stride;

  // PARSE STRIDE STRING
  struct user_stride ustride;
  if (compile_stride (&ustride, stride, e))
    {
      goto theend;
    }

  if (nsfslite_get_root (n, &rc, &vc, &temp, &params, e))
    {
      goto theend;
    }

  // RESOLVE STRIDE
  t_size size = type_byte_size (&params.t);
  if (stride_resolve (&_stride, ustride, rc->rptc.total_size / size, e))
    {
      rptc_cleanup (&rc->rptc, e);
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
    rptc_enter_transaction (&rc->rptc, tx);

    if (rptof_remove (
            &rc->rptc,
            dest,
            size,
            size * _stride.start,
            _stride.stride,
            _stride.nelems,
            e))
      {
        rptc_cleanup (&rc->rptc, e);
        goto theend;
      }

    rptc_leave_transaction (&rc->rptc);

    if (rptc_cleanup (&rc->rptc, e))
      {
        goto theend;
      }
  }

  // UPDATE VARIABLE META
  {
    varc_enter_transaction (&vc->vpc, tx);

    struct var_update_by_id_params uparams = {
      .id = id,
      .root = rc->rptc.root,
      .nbytes = rc->rptc.total_size,
    };
    if (vpc_update_by_id (&vc->vpc, &uparams, e))
      {
        goto theend;
      }
  }

  // COMMIT
  if (nsfslite_auto_commit (n, tx, auto_txn_started, e))
    {
      goto theend;
    }

theend:
  slab_alloc_free (&n->alloc, rc);
  slab_alloc_free (&n->alloc, vc);

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
