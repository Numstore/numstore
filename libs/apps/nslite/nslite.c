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
 *   Implements nslite.h. Core implementation of NumStore File System Lite, providing
 *   simplified database operations for managing named variables with transaction support.
 *   Handles cursor management, pager integration, and implements all CRUD operations
 *   with automatic or explicit transaction handling.
 */

#include <nslite.h>

#include <numstore/core/clock_allocator.h>
#include <numstore/core/error.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/pager.h>
#include <numstore/pager/lock_table.h>
#include <numstore/rptree/oneoff.h>
#include <numstore/rptree/rptree_cursor.h>

#include <pthread.h>

struct nslite_s
{
  struct pager *p;
  struct clck_alloc cursors;
  struct lockt lt;
  struct thread_pool *tp;
  struct latch l;
};

DEFINE_DBG_ASSERT (
    struct nslite_s, nslite, n, {
      ASSERT (n);
      ASSERT (n->p);
    })

nslite *
nslite_open (const char *fname, const char *recovery, error *e)
{
  // Allocate memory
  nslite *ret = i_malloc (1, sizeof *ret, e);
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
  ret->p = pgr_open (fname, recovery, &ret->lt, ret->tp, e);
  if (ret->p == NULL)
    {
      tp_free (ret->tp, e);
      lockt_destroy (&ret->lt);
      i_free (ret);
      goto failed;
    }

  // Open a clock allocator for cursors
  if (clck_alloc_open (&ret->cursors, sizeof (struct rptree_cursor), 512, e) < 0)
    {
      pgr_close (ret->p, e);
      tp_free (ret->tp, e);
      lockt_destroy (&ret->lt);
      i_free (ret);
      goto failed;
    }

  return ret;

failed:
  return NULL;
}

err_t
nslite_close (nslite *n, error *e)
{
  DBG_ASSERT (nslite, n);

  pgr_close (n->p, e);
  clck_alloc_close (&n->cursors);
  tp_free (n->tp, e);
  lockt_destroy (&n->lt);

  return e->cause_code;
}

// Higher Order Operations
spgno
nslite_new (nslite *n, nslite_txn *tx, error *e)
{
  DBG_ASSERT (nslite, n);

  int64_t ret = -1;

  // INIT
  struct rptree_cursor *rc = clck_alloc_alloc (&n->cursors, e);

  if (rc == NULL)
    {
      goto theend;
    }

  // BEGIN TXN
  struct txn auto_txn;
  bool auto_txn_started = false;

  if (tx == NULL)
    {
      if (pgr_begin_txn (&auto_txn, n->p, e))
        {
          goto theend;
        }
      auto_txn_started = true;
      tx = &auto_txn;
    }

  // CREATE RPT ROOT
  {
    if (rptc_new (rc, tx, n->p, &n->lt, e))
      {
        rptc_cleanup (rc, e);
        goto theend;
      }

    ret = rc->meta_root;
  }

  // COMMIT
  if (auto_txn_started)
    {
      if (pgr_commit (n->p, tx, e))
        {
          rptc_cleanup (rc, e);
          goto theend;
        }
    }

  // CLEANUP
  rptc_cleanup (rc, e);

theend:
  if (rc)
    {
      clck_alloc_free (&n->cursors, rc);
    }

  if (e->cause_code)
    {
      return e->cause_code;
    }

  return ret;
}

err_t
nslite_delete (nslite *n, nslite_txn *tx, pgno id, error *e)
{
  DBG_ASSERT (nslite, n);

  // INIT
  struct rptree_cursor *c = clck_alloc_alloc (&n->cursors, e);

  if (c == NULL)
    {
      goto theend;
    }

  // BEGIN TXN
  struct txn auto_txn;
  bool auto_txn_started = false;

  if (tx == NULL)
    {
      if (pgr_begin_txn (&auto_txn, n->p, e))
        {
          goto theend;
        }
      auto_txn_started = true;
      tx = &auto_txn;
    }

  // TODO - delete the rptree

  if (auto_txn_started)
    {
      if (pgr_commit (n->p, tx, e))
        {
          goto theend;
        }
    }

theend:
  if (c)
    {
      clck_alloc_free (&n->cursors, c);
    }

  return e->cause_code;
}

sb_size
nslite_size (nslite *n, pgno id, error *e)
{
  DBG_ASSERT (nslite, n);

  // INIT
  struct rptree_cursor *c = clck_alloc_alloc (&n->cursors, e);
  if (c == NULL)
    {
      goto theend;
    }

  // INIT RPTREE CURSOR with rpt_root page ID
  if (rptc_open (c, id, n->p, &n->lt, e))
    {
      goto theend;
    }

  b_size length = c->total_size;

  // CLEANUP
  if (rptc_cleanup (c, e))
    {
      goto theend;
    }

theend:
  if (c)
    {
      clck_alloc_free (&n->cursors, c);
    }

  if (e->cause_code)
    {
      return e->cause_code;
    }

  return length;
}

nslite_txn *
nslite_begin_txn (nslite *n, error *e)
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
nslite_commit (nslite *n, nslite_txn *tx, error *e)
{
  return pgr_commit (n->p, tx, e);
}

err_t
nslite_rollback (nslite *n, nslite_txn *tx, error *e)
{
  return pgr_rollback (n->p, tx, 0, e);
}

err_t
nslite_insert (
    nslite *n,
    pgno id,
    nslite_txn *tx,
    const void *src,
    b_size bofst,
    t_size size,
    b_size nelem,
    error *e)
{

  DBG_ASSERT (nslite, n);

  // INIT
  struct rptree_cursor *c = clck_alloc_alloc (&n->cursors, e);
  if (c == NULL)
    {
      goto theend;
    }

  // INIT RPTREE CURSOR with rpt_root page ID
  if (rptc_open (c, id, n->p, &n->lt, e))
    {
      goto theend;
    }

  struct txn auto_txn; // Maybe auto txn
  bool auto_txn_started = false;

  // BEGIN TXN
  if (tx == NULL)
    {
      if (pgr_begin_txn (&auto_txn, n->p, e))
        {
          goto theend;
        }
      auto_txn_started = true;
      tx = &auto_txn;
    }

  rptc_enter_transaction (c, tx);

  // DO INSERT
  if (rptof_insert (c, src, bofst, size, nelem, e))
    {
      goto theend;
    }

  // COMMIT
  rptc_leave_transaction (c);
  if (auto_txn_started)
    {
      if (pgr_commit (n->p, &auto_txn, e))
        {
          goto theend;
        }
    }

  // CLEANUP
  if (rptc_cleanup (c, e))
    {
      goto theend;
    }

theend:
  if (c)
    {
      clck_alloc_free (&n->cursors, c);
    }

  if (e->cause_code)
    {
      pgr_rollback (n->p, tx, 0, e);
    }

  return e->cause_code;
}

err_t
nslite_write (
    nslite *n,
    pgno id,
    nslite_txn *tx,
    const void *src,
    t_size size,
    struct nslite_stride stride,
    error *e)
{
  bool need_lock = (tx == NULL);

  DBG_ASSERT (nslite, n);

  // INIT
  struct rptree_cursor *c = clck_alloc_alloc (&n->cursors, e);
  if (c == NULL)
    {
      goto theend;
    }

  // INIT RPTREE CURSOR with rpt_root page ID
  if (rptc_open (c, id, n->p, &n->lt, e))
    {
      goto theend;
    }

  struct txn auto_txn;
  bool auto_txn_started = false;

  // BEGIN TXN
  if (tx == NULL)
    {
      if (pgr_begin_txn (&auto_txn, n->p, e))
        {
          goto theend;
        }
      auto_txn_started = true;
      tx = &auto_txn;
    }

  rptc_enter_transaction (c, tx);

  if (rptof_write (c, src, size, stride.bstart, stride.stride, stride.nelems, e))
    {
      goto theend;
    }

  // COMMIT
  rptc_leave_transaction (c);
  if (auto_txn_started)
    {
      if (pgr_commit (n->p, &auto_txn, e))
        {
          goto theend;
        }
    }

  // CLEANUP
  if (rptc_cleanup (c, e))
    {
      goto theend;
    }

theend:
  if (c)
    {
      clck_alloc_free (&n->cursors, c);
    }

  if (e->cause_code)
    {
      pgr_rollback (n->p, tx, 0, e);
    }

  return e->cause_code;
}

sb_size
nslite_read (
    nslite *n,
    pgno id,
    void *dest,
    t_size size,
    struct nslite_stride stride,
    error *e)
{
  DBG_ASSERT (nslite, n);

  // INIT
  struct rptree_cursor *c = clck_alloc_alloc (&n->cursors, e);
  if (c == NULL)
    {
      goto theend;
    }

  // INIT RPTREE CURSOR with rpt_root page ID
  if (rptc_open (c, id, n->p, &n->lt, e))
    {
      goto theend;
    }

  ssize_t ret = rptof_read (c, dest, size, stride.bstart, stride.stride, stride.nelems, e);
  if (ret < 0)
    {
      goto theend;
    }

  // CLEANUP
  if (rptc_cleanup (c, e))
    {
      goto theend;
    }

theend:
  if (c)
    {
      clck_alloc_free (&n->cursors, c);
    }

  if (e->cause_code)
    {
      return e->cause_code;
    }

  return ret;
}

err_t
nslite_remove (
    nslite *n,
    pgno id,
    nslite_txn *tx,
    void *dest,
    t_size size,
    struct nslite_stride stride,
    error *e)
{
  DBG_ASSERT (nslite, n);

  // INIT
  struct rptree_cursor *c = clck_alloc_alloc (&n->cursors, e);
  if (c == NULL)
    {
      goto theend;
    }

  // INIT RPTREE CURSOR with rpt_root page ID
  if (rptc_open (c, id, n->p, &n->lt, e))
    {
      goto theend;
    }

  struct txn auto_txn;
  bool auto_txn_started = false;

  // BEGIN TXN
  if (tx == NULL)
    {
      if (pgr_begin_txn (&auto_txn, n->p, e))
        {
          goto theend;
        }
      auto_txn_started = true;
      tx = &auto_txn;
    }

  rptc_enter_transaction (c, tx);

  if (rptof_remove (c, dest, size, stride.bstart, stride.stride, stride.nelems, e))
    {
      goto theend;
    }

  // COMMIT
  rptc_leave_transaction (c);
  if (auto_txn_started)
    {
      if (pgr_commit (n->p, &auto_txn, e))
        {
          goto theend;
        }
    }

  // CLEANUP
  if (rptc_cleanup (c, e))
    {
      goto theend;
    }

theend:
  if (c)
    {
      clck_alloc_free (&n->cursors, c);
    }

  if (e->cause_code)
    {
      pgr_rollback (n->p, tx, 0, e);
    }

  return e->cause_code;
}
