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

#include "numstore/rptree/oneoff.h"
#include <numstore/core/clock_allocator.h>
#include <numstore/core/error.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/pager.h>
#include <numstore/pager/lock_table.h>
#include <numstore/rptree/_rebalance.h>
#include <numstore/rptree/rptree_cursor.h>
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
  struct clck_alloc cursors;
  struct lockt lt;
  struct thread_pool *tp;
  struct latch l;
};

DEFINE_DBG_ASSERT (
    struct nsfslite_s, nsfslite, n, {
      ASSERT (n);
      ASSERT (n->p);
    })

nsfslite *
nsfslite_open (const char *fname, const char *recovery_fname, error *e)
{
  i_log_info ("nsfslite_open: fname=%s recovery=%s\n", fname, recovery_fname ? recovery_fname : "none");

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

  // Open a clock allocator for cursors
  if (clck_alloc_open (&ret->cursors, sizeof (union cursor), 512, e) < 0)
    {
      pgr_close (ret->p, e);
      tp_free (ret->tp, e);
      lockt_destroy (&ret->lt);
      i_free (ret);
      goto failed;
    }

  return ret;

failed:
  error_log_consume (e);
  return NULL;
}

err_t
nsfslite_close (nsfslite *n, error *e)
{
  DBG_ASSERT (nsfslite, n);

  pgr_close (n->p, e);
  clck_alloc_close (&n->cursors);
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
nsfslite_new (nsfslite *n, struct txn *tx, const char *name, error *e)
{
  DBG_ASSERT (nsfslite, n);

  i_log_info ("nsfslite_new: name=%s\n", name);

  int64_t ret = -1;

  // INIT
  union cursor *vc = clck_alloc_alloc (&n->cursors, e);
  union cursor *rc = clck_alloc_alloc (&n->cursors, e);

  if (vc == NULL || rc == NULL)
    {
      i_log_error ("nsfslite_new failed: cursor allocation error\n");
      goto theend;
    }

  if (varc_initialize (&vc->vpc, n->p, e) < 0)
    {
      i_log_error ("nsfslite_new failed: var cursor init error\n");
      goto theend;
    }

  // BEGIN TXN
  struct txn auto_txn;
  bool auto_txn_started = false;

  if (tx == NULL)
    {
      if (pgr_begin_txn (&auto_txn, n->p, e))
        {
          i_log_error ("nsfslite_new failed: begin txn error\n");
          varc_cleanup (&vc->vpc, e);
          goto theend;
        }
      auto_txn_started = true;
      tx = &auto_txn;
    }

  // CREATE RPT ROOT
  {
    if (rptc_new (&rc->rptc, tx, n->p, &n->lt, e))
      {
        varc_cleanup (&vc->vpc, e);
        rptc_cleanup (&rc->rptc, e);
        goto theend;
      }

    ret = rc->rptc.meta_root;
  }

  {
    varc_enter_transaction (&vc->vpc, tx);

    struct var_create_params src = {
      .vname = cstrfcstr (name),
      .t = (struct type){
          .type = T_PRIM,
          .p = U8,
      },
      .root = ret,
    };

    // CREATE VARIABLE
    if (vpc_new (&vc->vpc, src, e))
      {
        varc_cleanup (&vc->vpc, e);
        goto theend;
      }

    varc_leave_transaction (&vc->vpc);
  }

  // COMMIT
  if (auto_txn_started)
    {
      if (pgr_commit (n->p, tx, e))
        {
          varc_cleanup (&vc->vpc, e);
          rptc_cleanup (&rc->rptc, e);
          goto theend;
        }
    }

  // CLEANUP
  varc_cleanup (&vc->vpc, e);
  rptc_cleanup (&rc->rptc, e);
  if (e->cause_code)
    {
      i_log_error ("nsfslite_new failed: cleanup error code=%d\n", e->cause_code);
      goto theend;
    }

theend:
  if (vc)
    {
      clck_alloc_free (&n->cursors, vc);
    }
  if (rc)
    {
      clck_alloc_free (&n->cursors, rc);
    }

  if (e->cause_code)
    {
      ret = e->cause_code;
    }

  return ret;
}

spgno
nsfslite_get_id (nsfslite *n, const char *name, error *e)
{
  DBG_ASSERT (nsfslite, n);

  // INIT
  union cursor *c = clck_alloc_alloc (&n->cursors, e);
  if (c == NULL)
    {
      goto failed;
    }

  if (varc_initialize (&c->vpc, n->p, e) < 0)
    {
      goto failed;
    }

  // GET VARIABLE - Returns rpt_root page ID in pg0
  struct var_get_params params = {
    .vname = cstrfcstr (name),
  };

  if (vpc_get (&c->vpc, NULL, &params, e))
    {
      goto failed;
    }

  pgno id = params.pg0;

  // CLEANUP
  if (varc_cleanup (&c->vpc, e))
    {
      goto failed;
    }

  clck_alloc_free (&n->cursors, c);

  return id;

failed:
  if (c)
    {
      clck_alloc_free (&n->cursors, c);
    }

  return e->cause_code;
}

err_t
nsfslite_delete (nsfslite *n, struct txn *tx, const char *name, error *e)
{
  DBG_ASSERT (nsfslite, n);

  // INIT
  union cursor *c = clck_alloc_alloc (&n->cursors, e);

  if (c == NULL)
    {
      goto failed;
    }

  varc_initialize (&c->vpc, n->p, e);

  // BEGIN TXN
  struct txn auto_txn;
  bool auto_txn_started = false;

  if (tx == NULL)
    {
      if (pgr_begin_txn (&auto_txn, n->p, e))
        {
          goto failed;
        }
      auto_txn_started = true;
      tx = &auto_txn;
    }

  varc_enter_transaction (&c->vpc, tx);

  // DELETE VARIABLE
  if (vpc_delete (&c->vpc, cstrfcstr (name), e))
    {
      goto failed;
    }

  // TODO - delete the rptree

  // COMMIT
  varc_leave_transaction (&c->vpc);
  if (auto_txn_started)
    {
      if (pgr_commit (n->p, tx, e))
        {
          goto failed;
        }
    }

  // CLEAN UP
  if (varc_cleanup (&c->vpc, e))
    {
      goto failed;
    }

  clck_alloc_free (&n->cursors, &c->vpc);

  return SUCCESS;

failed:
  if (c)
    {
      clck_alloc_free (&n->cursors, c);
    }
  return e->cause_code;
}

sb_size
nsfslite_fsize (nsfslite *n, pgno id, error *e)
{

  DBG_ASSERT (nsfslite, n);

  // INIT
  union cursor *c = clck_alloc_alloc (&n->cursors, e);
  if (c == NULL)
    {
      goto failed;
    }

  // INIT RPTREE CURSOR with rpt_root page ID
  if (rptc_open (&c->rptc, id, n->p, &n->lt, e))
    {
      goto failed;
    }

  b_size length = c->rptc.total_size;

  // CLEANUP
  if (rptc_cleanup (&c->rptc, e))
    {
      goto failed;
    }

  clck_alloc_free (&n->cursors, c);

  return length;

failed:
  if (c)
    {
      clck_alloc_free (&n->cursors, c);
    }

  return e->cause_code;
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

int
nsfslite_commit (nsfslite *n, struct txn *tx, error *e)
{
  int ret = pgr_commit (n->p, tx, e);

  return ret;
}

err_t
nsfslite_insert (
    nsfslite *n,
    pgno id,
    struct txn *tx,
    const void *src,
    b_size bofst,
    t_size size,
    b_size nelem,
    error *e)
{

  DBG_ASSERT (nsfslite, n);

  // INIT
  union cursor *c = clck_alloc_alloc (&n->cursors, e);
  if (c == NULL)
    {
      i_log_warn ("nsfslite_insert failed: cursor allocation error\n");
      goto failed;
    }

  // INIT RPTREE CURSOR with rpt_root page ID
  if (rptc_open (&c->rptc, id, n->p, &n->lt, e))
    {
      i_log_warn ("nsfslite_insert failed: rptc_open error id=%" PRIu64 "\n", id);
      goto failed;
    }

  struct txn auto_txn; // Maybe auto txn
  bool auto_txn_started = false;

  // BEGIN TXN
  if (tx == NULL)
    {
      if (pgr_begin_txn (&auto_txn, n->p, e))
        {
          goto failed;
        }
      auto_txn_started = true;
      tx = &auto_txn;
      i_log_trace ("nsfslite_insert: created implicit tx=%" PRIu64 "\n", auto_txn.tid);
    }

  rptc_enter_transaction (&c->rptc, tx);

  // DO INSERT
  if (rptof_insert (&c->rptc, src, bofst, size, nelem, e))
    {
      goto failed;
    }

  // COMMIT
  rptc_leave_transaction (&c->rptc);
  if (auto_txn_started)
    {
      if (pgr_commit (n->p, &auto_txn, e))
        {
          goto failed;
        }
    }

  // CLEANUP
  if (rptc_cleanup (&c->rptc, e))
    {
      goto failed;
    }

  clck_alloc_free (&n->cursors, c);

  i_log_trace ("nsfslite_insert: success id=%" PRIu64 " tx=%" PRIu64 " inserted=%zu\n", id, tx->tid, nelem);
  return SUCCESS;

failed:
  if (c)
    {
      clck_alloc_free (&n->cursors, c);
    }

  pgr_rollback (n->p, tx, 0, e);

  return e->cause_code;
}

err_t
nsfslite_write (
    nsfslite *n,
    pgno id,
    struct txn *tx,
    const void *src,
    t_size size,
    struct stride stride,
    error *e)
{
  DBG_ASSERT (nsfslite, n);

  // INIT
  union cursor *c = clck_alloc_alloc (&n->cursors, e);
  if (c == NULL)
    {
      goto failed;
    }

  // INIT RPTREE CURSOR with rpt_root page ID
  if (rptc_open (&c->rptc, id, n->p, &n->lt, e))
    {
      goto failed;
    }

  struct txn auto_txn;
  bool auto_txn_started = false;

  // BEGIN TXN
  if (tx == NULL)
    {
      if (pgr_begin_txn (&auto_txn, n->p, e))
        {
          goto failed;
        }
      auto_txn_started = true;
      tx = &auto_txn;
    }

  rptc_enter_transaction (&c->rptc, tx);

  if (rptof_write (&c->rptc, src, size, stride.bstart, stride.stride, stride.nelems, e))
    {
      goto failed;
    }

  // COMMIT
  rptc_leave_transaction (&c->rptc);
  if (auto_txn_started)
    {
      if (pgr_commit (n->p, &auto_txn, e))
        {
          goto failed;
        }
    }

  // CLEANUP
  if (rptc_cleanup (&c->rptc, e))
    {
      goto failed;
    }

  clck_alloc_free (&n->cursors, c);

  return stride.nelems;

failed:
  if (c)
    {
      clck_alloc_free (&n->cursors, c);
    }

  pgr_rollback (n->p, tx, 0, e);

  return e->cause_code;
}

sb_size
nsfslite_read (
    nsfslite *n,
    pgno id,
    void *dest,
    t_size size,
    struct stride stride,
    error *e)
{
  DBG_ASSERT (nsfslite, n);

  // INIT
  union cursor *c = clck_alloc_alloc (&n->cursors, e);
  if (c == NULL)
    {
      goto failed;
    }

  // INIT RPTREE CURSOR with rpt_root page ID
  if (rptc_open (&c->rptc, id, n->p, &n->lt, e))
    {
      goto failed;
    }

  sb_size ret = rptof_read (&c->rptc, dest, size, stride.bstart, stride.stride, stride.nelems, e);
  if (ret < 0)
    {
      goto failed;
    }

  // CLEANUP
  if (rptc_cleanup (&c->rptc, e))
    {
      goto failed;
    }

  clck_alloc_free (&n->cursors, c);

  i_log_trace ("nsfslite_read: success id=%" PRIu64 " read=%zd\n", id, ret);
  return ret;

failed:
  if (c)
    {
      clck_alloc_free (&n->cursors, c);
    }

  i_log_warn ("nsfslite_read failed: id=%" PRIu64 " code=%d\n", id, e->cause_code);
  return e->cause_code;
}

err_t
nsfslite_remove (
    nsfslite *n,
    pgno id,
    struct txn *tx,
    void *dest,
    t_size size,
    struct stride stride,
    error *e)
{

  DBG_ASSERT (nsfslite, n);

  // INIT
  union cursor *c = clck_alloc_alloc (&n->cursors, e);
  if (c == NULL)
    {
      goto failed;
    }

  // INIT RPTREE CURSOR with rpt_root page ID
  if (rptc_open (&c->rptc, id, n->p, &n->lt, e))
    {
      goto failed;
    }

  struct txn auto_txn;
  bool auto_txn_started = false;

  // BEGIN TXN
  if (tx == NULL)
    {
      if (pgr_begin_txn (&auto_txn, n->p, e))
        {
          goto failed;
        }
      auto_txn_started = true;
      tx = &auto_txn;
    }

  rptc_enter_transaction (&c->rptc, tx);

  if (rptof_remove (&c->rptc, dest, size, stride.bstart, stride.stride, stride.nelems, e))
    {
      goto failed;
    }

  // COMMIT
  rptc_leave_transaction (&c->rptc);
  if (auto_txn_started)
    {
      if (pgr_commit (n->p, &auto_txn, e))
        {
          goto failed;
        }
    }

  // CLEANUP
  if (rptc_cleanup (&c->rptc, e))
    {
      goto failed;
    }

  clck_alloc_free (&n->cursors, c);

  i_log_trace ("nsfslite_remove: success id=%" PRIu64 " tx=%" PRIu64 " removed=%zd\n", id, tx->tid, removed);

  return SUCCESS;

failed:
  if (c)
    {
      clck_alloc_free (&n->cursors, c);
    }

  pgr_rollback (n->p, tx, 0, e);

  return e->cause_code;
}
