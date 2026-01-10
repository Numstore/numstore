/*
 * Copyright 2025 Theo Lincke
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 *   Implements nsfslite.h. Core implementation of NumStore File System Lite, providing
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
  error e;
};

DEFINE_DBG_ASSERT (
    struct nsfslite_s, nsfslite, n, {
      ASSERT (n);
      ASSERT (n->p);
    })

const char *
nsfslite_error (nsfslite *n)
{
  if (n->e.cause_code == SUCCESS)
    {
      return "nsfslite OK!\n";
    }
  else
    {
      return n->e.cause_msg;
    }
}

void
nsfslite_reset_errors (nsfslite *n)
{
  error_reset (&n->e);
}

nsfslite *
nsfslite_open (const char *fname, const char *recovery_fname)
{
  error e = error_create ();

  i_log_info ("nsfslite_open: fname=%s recovery=%s\n", fname, recovery_fname ? recovery_fname : "none");

  // Allocate memory
  nsfslite *ret = i_malloc (1, sizeof *ret, &e);
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
  ret->p = pgr_open (fname, recovery_fname, &ret->lt, ret->tp, &e);
  if (ret->p == NULL)
    {
      tp_free (ret->tp, &e);
      lockt_destroy (&ret->lt);
      i_free (ret);
      goto failed;
    }

  if (pgr_get_npages (ret->p) == 1)
    {
      // Create a variable hash table page
      if (varh_init_hash_page (ret->p, &e) < 0)
        {
          pgr_close (ret->p, &e);
          i_free (ret);
          goto failed;
        }
    }

  // Open a clock allocator for cursors
  if (clck_alloc_open (&ret->cursors, sizeof (union cursor), 512, &e) < 0)
    {
      pgr_close (ret->p, &e);
      tp_free (ret->tp, &e);
      lockt_destroy (&ret->lt);
      i_free (ret);
      goto failed;
    }

  ret->e = e;

#ifndef NDEBUG
  ret->e.print_trace = true;
#endif

  return ret;

failed:
  error_log_consume (&e);
  return NULL;
}

int
nsfslite_close (nsfslite *n)
{
  DBG_ASSERT (nsfslite, n);
  error_reset (&n->e);

  pgr_close (n->p, &n->e);
  clck_alloc_close (&n->cursors);
  tp_free (n->tp, &n->e);
  lockt_destroy (&n->lt);

  if (n->e.cause_code < 0)
    {
      i_log_error ("nsfslite_close failed: code=%d\n", n->e.cause_code);
      return n->e.cause_code;
    }

  i_log_debug ("nsfslite_close: success\n");
  return SUCCESS;
}

// Higher Order Operations
int64_t
nsfslite_new (nsfslite *n, nsfslite_txn *tx, const char *name)
{
  DBG_ASSERT (nsfslite, n);
  error_reset (&n->e);

  i_log_info ("nsfslite_new: name=%s\n", name);

  int64_t ret = -1;

  // INIT
  union cursor *vc = clck_alloc_alloc (&n->cursors, &n->e);
  union cursor *rc = clck_alloc_alloc (&n->cursors, &n->e);

  if (vc == NULL || rc == NULL)
    {
      i_log_error ("nsfslite_new failed: cursor allocation error\n");
      goto theend;
    }

  if (varc_initialize (&vc->vpc, n->p, &n->e) < 0)
    {
      i_log_error ("nsfslite_new failed: var cursor init error\n");
      goto theend;
    }

  // BEGIN TXN
  struct txn auto_txn;
  bool auto_txn_started = false;

  if (tx == NULL)
    {
      if (pgr_begin_txn (&auto_txn, n->p, &n->e))
        {
          i_log_error ("nsfslite_new failed: begin txn error\n");
          varc_cleanup (&vc->vpc, &n->e);
          goto theend;
        }
      auto_txn_started = true;
      tx = &auto_txn;
    }

  // CREATE RPT ROOT
  {
    if (rptc_new (&rc->rptc, tx, n->p, &n->e))
      {
        varc_cleanup (&vc->vpc, &n->e);
        rptc_cleanup (&rc->rptc, &n->e);
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
    if (vpc_new (&vc->vpc, src, &n->e))
      {
        varc_cleanup (&vc->vpc, &n->e);
        goto theend;
      }

    varc_leave_transaction (&vc->vpc);
  }

  // COMMIT
  if (auto_txn_started)
    {
      if (pgr_commit (n->p, tx, &n->e))
        {
          varc_cleanup (&vc->vpc, &n->e);
          rptc_cleanup (&rc->rptc, &n->e);
          goto theend;
        }
    }

  // CLEANUP
  varc_cleanup (&vc->vpc, &n->e);
  rptc_cleanup (&rc->rptc, &n->e);
  if (n->e.cause_code)
    {
      i_log_error ("nsfslite_new failed: cleanup error code=%d\n", n->e.cause_code);
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

  if (n->e.cause_code)
    {
      ret = n->e.cause_code;
    }

  return ret;
}

int64_t
nsfslite_get_id (nsfslite *n, const char *name)
{

  DBG_ASSERT (nsfslite, n);
  error_reset (&n->e);

  // INIT
  union cursor *c = clck_alloc_alloc (&n->cursors, &n->e);
  if (c == NULL)
    {
      goto failed;
    }

  if (varc_initialize (&c->vpc, n->p, &n->e) < 0)
    {
      goto failed;
    }

  // GET VARIABLE - Returns rpt_root page ID in pg0
  struct var_get_params params = {
    .vname = cstrfcstr (name),
  };

  if (vpc_get (&c->vpc, NULL, &params, &n->e))
    {
      goto failed;
    }

  pgno id = params.pg0;

  // CLEANUP
  if (varc_cleanup (&c->vpc, &n->e))
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

  return n->e.cause_code;
}

int
nsfslite_delete (nsfslite *n, nsfslite_txn *tx, const char *name)
{

  DBG_ASSERT (nsfslite, n);
  error_reset (&n->e);

  // INIT
  union cursor *c = clck_alloc_alloc (&n->cursors, &n->e);

  if (c == NULL)
    {
      goto failed;
    }

  varc_initialize (&c->vpc, n->p, &n->e);

  // BEGIN TXN
  struct txn auto_txn;
  bool auto_txn_started = false;

  if (tx == NULL)
    {
      if (pgr_begin_txn (&auto_txn, n->p, &n->e))
        {
          goto failed;
        }
      auto_txn_started = true;
      tx = &auto_txn;
    }

  varc_enter_transaction (&c->vpc, tx);

  // DELETE VARIABLE
  if (vpc_delete (&c->vpc, cstrfcstr (name), &n->e))
    {
      goto failed;
    }

  // TODO - delete the rptree

  // COMMIT
  varc_leave_transaction (&c->vpc);
  if (auto_txn_started)
    {
      if (pgr_commit (n->p, tx, &n->e))
        {
          goto failed;
        }
    }

  // CLEAN UP
  if (varc_cleanup (&c->vpc, &n->e))
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
  return n->e.cause_code;
}

size_t
nsfslite_fsize (nsfslite *n, uint64_t id)
{

  DBG_ASSERT (nsfslite, n);
  error_reset (&n->e);

  // INIT
  union cursor *c = clck_alloc_alloc (&n->cursors, &n->e);
  if (c == NULL)
    {
      goto failed;
    }

  // INIT RPTREE CURSOR with rpt_root page ID
  if (rptc_open (&c->rptc, id, n->p, &n->e))
    {
      goto failed;
    }

  b_size length = c->rptc.total_size;

  // CLEANUP
  if (rptc_cleanup (&c->rptc, &n->e))
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

  return n->e.cause_code;
}

nsfslite_txn *
nsfslite_begin_txn (nsfslite *n)
{
  error e = error_create ();

  struct txn *tx = i_malloc (1, sizeof *tx, &e);
  if (tx == NULL)
    {
      error_log_consume (&e);
      return NULL;
    }

  if (pgr_begin_txn (tx, n->p, &n->e))
    {
      error_log_consume (&e);
      return NULL;
    }

  return tx;
}

int
nsfslite_commit (nsfslite *n, nsfslite_txn *tx)
{
  int ret = pgr_commit (n->p, tx, &n->e);

  return ret;
}

ssize_t
nsfslite_insert (
    nsfslite *n,
    uint64_t id,
    nsfslite_txn *tx,
    const void *src,
    size_t bofst,
    size_t size,
    size_t nelem)
{

  DBG_ASSERT (nsfslite, n);
  error_reset (&n->e);

  // INIT
  union cursor *c = clck_alloc_alloc (&n->cursors, &n->e);
  if (c == NULL)
    {
      i_log_warn ("nsfslite_insert failed: cursor allocation error\n");
      goto failed;
    }

  // INIT RPTREE CURSOR with rpt_root page ID
  if (rptc_open (&c->rptc, id, n->p, &n->e))
    {
      i_log_warn ("nsfslite_insert failed: rptc_open error id=%" PRIu64 "\n", id);
      goto failed;
    }

  struct txn auto_txn; // Maybe auto txn
  bool auto_txn_started = false;

  // BEGIN TXN
  if (tx == NULL)
    {
      if (pgr_begin_txn (&auto_txn, n->p, &n->e))
        {
          goto failed;
        }
      auto_txn_started = true;
      tx = &auto_txn;
      i_log_trace ("nsfslite_insert: created implicit tx=%" PRIu64 "\n", auto_txn.tid);
    }

  rptc_enter_transaction (&c->rptc, tx);

  // DO INSERT
  if (rptof_insert (&c->rptc, src, bofst, size, nelem, &n->e))
    {
      goto failed;
    }

  // COMMIT
  rptc_leave_transaction (&c->rptc);
  if (auto_txn_started)
    {
      if (pgr_commit (n->p, &auto_txn, &n->e))
        {
          goto failed;
        }
    }

  // CLEANUP
  if (rptc_cleanup (&c->rptc, &n->e))
    {
      goto failed;
    }

  clck_alloc_free (&n->cursors, c);

  i_log_trace ("nsfslite_insert: success id=%" PRIu64 " tx=%" PRIu64 " inserted=%zu\n", id, tx->tid, nelem);
  return nelem;

failed:
  if (c)
    {
      clck_alloc_free (&n->cursors, c);
    }

  pgr_rollback (n->p, tx, 0, &n->e);

  return n->e.cause_code;
}

ssize_t
nsfslite_write (
    nsfslite *n,
    uint64_t id,
    nsfslite_txn *tx,
    const void *src,
    size_t size,
    struct nsfslite_stride stride)
{
  bool need_lock = (tx == NULL);

  DBG_ASSERT (nsfslite, n);
  error_reset (&n->e);

  // INIT
  union cursor *c = clck_alloc_alloc (&n->cursors, &n->e);
  if (c == NULL)
    {
      goto failed;
    }

  // INIT RPTREE CURSOR with rpt_root page ID
  if (rptc_open (&c->rptc, id, n->p, &n->e))
    {
      goto failed;
    }

  struct txn auto_txn;
  bool auto_txn_started = false;

  // BEGIN TXN
  if (tx == NULL)
    {
      if (pgr_begin_txn (&auto_txn, n->p, &n->e))
        {
          goto failed;
        }
      auto_txn_started = true;
      tx = &auto_txn;
    }

  rptc_enter_transaction (&c->rptc, tx);

  if (rptof_write (&c->rptc, src, size, stride.bstart, stride.stride, stride.nelems, &n->e))
    {
      goto failed;
    }

  // COMMIT
  rptc_leave_transaction (&c->rptc);
  if (auto_txn_started)
    {
      if (pgr_commit (n->p, &auto_txn, &n->e))
        {
          goto failed;
        }
    }

  // CLEANUP
  if (rptc_cleanup (&c->rptc, &n->e))
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

  pgr_rollback (n->p, tx, 0, &n->e);

  return n->e.cause_code;
}

ssize_t
nsfslite_read (
    nsfslite *n,
    uint64_t id,
    void *dest,
    size_t size,
    struct nsfslite_stride stride)
{

  DBG_ASSERT (nsfslite, n);
  error_reset (&n->e);

  i_log_debug ("nsfslite_read: id=%" PRIu64 " bstart=%zu stride=%zu nelems=%zu size=%zu\n",
               id, stride.bstart, stride.stride, stride.nelems, size);

  // INIT
  union cursor *c = clck_alloc_alloc (&n->cursors, &n->e);
  if (c == NULL)
    {
      goto failed;
    }

  // INIT RPTREE CURSOR with rpt_root page ID
  if (rptc_open (&c->rptc, id, n->p, &n->e))
    {
      goto failed;
    }

  ssize_t ret = rptof_read (&c->rptc, dest, size, stride.bstart, stride.stride, stride.nelems, &n->e);
  if (ret < 0)
    {
      goto failed;
    }

  // CLEANUP
  if (rptc_cleanup (&c->rptc, &n->e))
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

  i_log_warn ("nsfslite_read failed: id=%" PRIu64 " code=%d\n", id, n->e.cause_code);
  return n->e.cause_code;
}

ssize_t
nsfslite_remove (
    nsfslite *n,
    uint64_t id,
    nsfslite_txn *tx,
    void *dest,
    size_t size,
    struct nsfslite_stride stride)
{

  DBG_ASSERT (nsfslite, n);
  error_reset (&n->e);

  // INIT
  union cursor *c = clck_alloc_alloc (&n->cursors, &n->e);
  if (c == NULL)
    {
      goto failed;
    }

  // INIT RPTREE CURSOR with rpt_root page ID
  if (rptc_open (&c->rptc, id, n->p, &n->e))
    {
      goto failed;
    }

  struct txn auto_txn;
  bool auto_txn_started = false;

  // BEGIN TXN
  if (tx == NULL)
    {
      if (pgr_begin_txn (&auto_txn, n->p, &n->e))
        {
          goto failed;
        }
      auto_txn_started = true;
      tx = &auto_txn;
    }

  rptc_enter_transaction (&c->rptc, tx);

  if (rptof_remove (&c->rptc, dest, size, stride.bstart, stride.stride, stride.nelems, &n->e))
    {
      goto failed;
    }

  // COMMIT
  rptc_leave_transaction (&c->rptc);
  if (auto_txn_started)
    {
      if (pgr_commit (n->p, &auto_txn, &n->e))
        {
          goto failed;
        }
    }

  // CLEANUP
  if (rptc_cleanup (&c->rptc, &n->e))
    {
      goto failed;
    }

  clck_alloc_free (&n->cursors, c);

  i_log_trace ("nsfslite_remove: success id=%" PRIu64 " tx=%" PRIu64 " removed=%zd\n", id, tx->tid, removed);

  return stride.nelems;

failed:
  if (c)
    {
      clck_alloc_free (&n->cursors, c);
    }

  pgr_rollback (n->p, tx, 0, &n->e);

  return n->e.cause_code;
}
