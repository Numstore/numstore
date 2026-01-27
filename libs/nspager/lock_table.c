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
 *   TODO: Add description for lock_table.c
 */

#include "numstore/core/slab_alloc.h"
#include <numstore/core/latch.h>
#include <numstore/intf/os/threading.h>
#include <numstore/pager/lock_table.h>

#include <numstore/core/adptv_hash_table.h>
#include <numstore/core/assert.h>
#include <numstore/core/clock_allocator.h>
#include <numstore/core/error.h>
#include <numstore/core/gr_lock.h>
#include <numstore/core/hash_table.h>
#include <numstore/core/hashing.h>
#include <numstore/core/ht_models.h>
#include <numstore/core/threadpool.h>
#include <numstore/intf/os/file_system.h>
#include <numstore/pager.h>
#include <numstore/pager/lt_lock.h>
#include <numstore/pager/txn.h>

struct lockt_frame
{
  struct lt_lock key;
  struct gr_lock lock;
  struct hnode node;
};

static err_t
lockt_frame_init (struct lockt_frame *dest, struct lt_lock key, error *e)
{
  err_t_wrap (gr_lock_init (&dest->lock, e), e);

  dest->key = key;
  hnode_init (&dest->node, lt_lock_key (key));

  return SUCCESS;
}

static void
lockt_frame_init_key (struct lockt_frame *dest, struct lt_lock key)
{
  dest->key = key;
  hnode_init (&dest->node, lt_lock_key (key));
}

static bool
lockt_frame_eq (const struct hnode *left, const struct hnode *right)
{
  const struct lockt_frame *_left = container_of (left, struct lockt_frame, node);
  const struct lockt_frame *_right = container_of (right, struct lockt_frame, node);
  return lt_lock_equal (_left->key, _right->key);
}

err_t
lockt_init (struct lockt *t, error *e)
{
  slab_alloc_init (&t->lock_alloc, sizeof (struct lockt_frame), 1000);

  struct adptv_htable_settings settings = {
    .max_load_factor = 8,
    .min_load_factor = 1,
    .rehashing_work = 28,
    .max_size = 2048,
    .min_size = 10,
  };

  if (adptv_htable_init (&t->table, settings, e))
    {
      return e->cause_code;
    }

  latch_init (&t->l);

  return SUCCESS;
}

void
lockt_destroy (struct lockt *t)
{
  // TODO - wait for all locks?
  slab_alloc_destroy (&t->lock_alloc);
  adptv_htable_free (&t->table);
}

#ifndef NDEBUG
static bool
is_tx_lock (enum lock_mode mode)
{
  switch (mode)
    {
    case LM_IS:
    case LM_S:
      {
        return false;
      }
    case LM_IX:
    case LM_SIX:
    case LM_X:
      {
        return true;
      }
    case LM_COUNT:
      {
        UNREACHABLE ();
      }
    }
  UNREACHABLE ();
}
#endif

static err_t
lockt_lock_once (
    struct lockt *t,
    struct lt_lock lock,
    enum lock_mode mode,
    struct txn *tx,
    error *e)
{
  if (tx && txn_haslock (tx, lock))
    {
      return SUCCESS;
    }

  struct lockt_frame *frame = NULL;

  latch_lock (&t->l);

  // Look up this resource in the lock table
  struct lockt_frame key;
  lockt_frame_init_key (&key, lock);
  struct hnode *node = adptv_htable_lookup (&t->table, &key.node, lockt_frame_eq);

  // Lock already exists
  if (node != NULL)
    {
      frame = container_of (node, struct lockt_frame, node);
      // TODO check if this lock belongs to the same transaction
    }

  // Lock doesn't exist - create new one
  else
    {
      // Allocate New
      frame = slab_alloc_alloc (&t->lock_alloc, e);
      err_t_panic (e->cause_code, e);
      err_t_panic (lockt_frame_init (frame, lock, e), e);
      err_t_panic (adptv_htable_insert (&t->table, &frame->node, e), e);
    }

  gr_lock_incref (&frame->lock);

  if (tx)
    {
      err_t_panic (txn_newlock (tx, lock, mode, e), e);
    }

  // Release latch BEFORE blocking on gr_lock
  latch_unlock (&t->l);

  // Now acquire the gr_lock (may block here)
  err_t_panic (gr_lock (&frame->lock, mode, e), e);

  return SUCCESS;
}

err_t
lockt_lock (
    struct lockt *t,
    struct lt_lock lock,
    enum lock_mode mode,
    struct txn *tx,
    error *e)
{
  ASSERT (tx || !is_tx_lock (mode));

  // First you need to obtain a lock on the parent
  struct lt_lock parent;

  if (get_parent (&parent, lock))
    {
      enum lock_mode pmode = get_parent_mode (mode);

      err_t_wrap (lockt_lock (t, parent, pmode, tx, e), e);
    }

  // Then lock this node
  return lockt_lock_once (t, lock, mode, tx, e);
}

static inline err_t
lockt_unlock_and_maybe_remove_unsafe (
    struct lockt *t,
    struct lt_lock lock,
    enum lock_mode mode,
    error *e)
{
  struct lockt_frame key;
  lockt_frame_init_key (&key, lock);

  struct hnode *found = adptv_htable_lookup (&t->table, &key.node, lockt_frame_eq);
  ASSERT (found);
  struct lockt_frame *frame = container_of (found, struct lockt_frame, node);

  gr_unlock (&frame->lock, mode);

  if (gr_lock_decref (&frame->lock))
    {
      err_t_wrap (adptv_htable_delete (NULL, &t->table, &key.node, lockt_frame_eq, e), e);
      gr_lock_destroy (&frame->lock);
      slab_alloc_free (&t->lock_alloc, frame);
    }

  return SUCCESS;
}

struct unlock_ctx
{
  struct lockt *t;
};

static err_t
unlock_and_maybe_remove (struct lt_lock lock, enum lock_mode mode, void *ctx, error *e)
{
  struct unlock_ctx *c = ctx;

  lockt_unlock_and_maybe_remove_unsafe (c->t, lock, mode, e);

  return SUCCESS;
}

static err_t
lockt_unlock_once (
    struct lockt *t,
    struct lt_lock lock,
    enum lock_mode mode, error *e)
{
  latch_lock (&t->l);

  err_t ret = lockt_unlock_and_maybe_remove_unsafe (t, lock, mode, e);

  latch_unlock (&t->l);

  return ret;
}

err_t
lockt_unlock (struct lockt *t, struct lt_lock lock, enum lock_mode mode, error *e)
{
  // First, unlock the child
  lockt_unlock_once (t, lock, mode, e);

  // Next, you need to unlock the parent
  struct lt_lock parent;

  if (get_parent (&parent, lock))
    {
      enum lock_mode pmode = get_parent_mode (mode);
      lockt_unlock (t, parent, pmode, e);
    }

  return e->cause_code;
}

err_t
lockt_unlock_tx (struct lockt *t, struct txn *tx, error *e)
{
  ASSERT (t);
  ASSERT (tx);

  latch_lock (&t->l);
  err_t_wrap (txn_foreach_lock (tx, unlock_and_maybe_remove, &(struct unlock_ctx){ .t = t }, e), e);
  latch_unlock (&t->l);

  txn_free_all_locks (tx);

  return SUCCESS;
}

static void
i_log_lockt_frame_hnode (struct hnode *node, void *ctx)
{
  int *log_level = ctx;
  struct lockt_frame *frame = container_of (node, struct lockt_frame, node);
  i_print_lt_lock (*log_level, frame->key);
}

void
i_log_lockt (int log_level, struct lockt *t)
{
  i_log (log_level, "================== LOCK TABLE START ==================\n");
  adptv_htable_foreach (&t->table, i_log_lockt_frame_hnode, &log_level);
  i_log (log_level, "================== LOCK TABLE END ==================\n");
}

#ifndef NTEST

struct lt_lock_parent
{
  enum lt_lock_type type;
  union lt_lock_data data;
  enum lock_mode mode;
};

/**
// Helper function to verify lock hierarchy
static void
verify_lock_and_commit (
    struct lockt *lt,
    struct pager *p,
    enum lt_lock_type lock_type,
    union lt_lock_data lock_data,
    enum lock_mode mode,
    struct lt_lock_parent *expected_parents,
    size_t num_parents,
    error *e)
{
  struct txn tx;
  struct lt_lock key;
  struct hnode *node;
  struct lt_lock *lock;

  test_err_t_wrap (pgr_begin_txn (&tx, p, e), e);

  lock = lockt_lock (lt, lock_type, lock_data, mode, &tx, e);
  test_fail_if_null (lock);

  // Verify all expected locks exist
  {
    // This lock
    lt_lock_init_key (&key, lock_type, lock_data);
    node = adptv_htable_lookup (&lt->table, &key.node, lt_lock_eq);
    test_fail_if_null (node);
    lock = container_of (node, struct lt_lock, node);
    test_assert_int_equal (lock->mode, mode);

    // Parent locks
    for (size_t i = 0; i < num_parents; i++)
      {
        lt_lock_init_key (&key, expected_parents[i].type, expected_parents[i].data);
        node = adptv_htable_lookup (&lt->table, &key.node, lt_lock_eq);
        test_fail_if_null (node);
        lock = container_of (node, struct lt_lock, node);
        test_assert_int_equal (lock->mode, expected_parents[i].mode);
      }

    // Verify ONLY these locks
    test_assert_int_equal (adptv_htable_size (&lt->table), num_parents + 1);
  }

  test_err_t_wrap (pgr_commit (p, &tx, e), e);

  {
    // This lock
    lt_lock_init_key (&key, lock_type, lock_data);
    node = adptv_htable_lookup (&lt->table, &key.node, lt_lock_eq);
    test_assert_equal (node, NULL);

    // Parent locks
    for (size_t i = 0; i < num_parents; i++)
      {
        lt_lock_init_key (&key, expected_parents[i].type, expected_parents[i].data);
        node = adptv_htable_lookup (&lt->table, &key.node, lt_lock_eq);
        test_assert_equal (node, NULL);
      }

    // Verify all locks removed
    test_assert_int_equal (adptv_htable_size (&lt->table), 0);
  }
}

TEST (TT_UNIT, lockt_lock)
{
  error e = error_create ();

  test_err_t_wrap (i_remove_quiet ("test.db", &e), &e);
  test_err_t_wrap (i_remove_quiet ("test.wal", &e), &e);

  struct lockt lt;
  struct thread_pool *tp = tp_open (&e);
  test_err_t_wrap (lockt_init (&lt, &e), &e);
  struct pager *p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_fail_if_null (p);

  // LOCK_DB
  TEST_CASE ("DB LM_IS")
  {
    verify_lock_and_commit (&lt, p, LOCK_DB, (union lt_lock_data){ 0 }, LM_IS, NULL, 0, &e);
  }

  TEST_CASE ("DB LM_IX")
  {
    verify_lock_and_commit (&lt, p, LOCK_DB, (union lt_lock_data){ 0 }, LM_IX, NULL, 0, &e);
  }

  TEST_CASE ("DB LM_S")
  {
    verify_lock_and_commit (&lt, p, LOCK_DB, (union lt_lock_data){ 0 }, LM_S, NULL, 0, &e);
  }

  TEST_CASE ("DB LM_SIX")
  {
    verify_lock_and_commit (&lt, p, LOCK_DB, (union lt_lock_data){ 0 }, LM_SIX, NULL, 0, &e);
  }

  TEST_CASE ("DB LM_X")
  {
    verify_lock_and_commit (&lt, p, LOCK_DB, (union lt_lock_data){ 0 }, LM_X, NULL, 0, &e);
  }

  // LOCK_ROOT
  TEST_CASE ("ROOT LM_IS")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IS } };
    verify_lock_and_commit (&lt, p, LOCK_ROOT, (union lt_lock_data){ 0 }, LM_IS, parents, 1, &e);
  }

  TEST_CASE ("ROOT LM_IX")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IX } };
    verify_lock_and_commit (&lt, p, LOCK_ROOT, (union lt_lock_data){ 0 }, LM_IX, parents, 1, &e);
  }

  TEST_CASE ("ROOT LM_S")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IS } };
    verify_lock_and_commit (&lt, p, LOCK_ROOT, (union lt_lock_data){ 0 }, LM_S, parents, 1, &e);
  }

  TEST_CASE ("ROOT LM_SIX")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IX } };
    verify_lock_and_commit (&lt, p, LOCK_ROOT, (union lt_lock_data){ 0 }, LM_SIX, parents, 1, &e);
  }

  TEST_CASE ("ROOT LM_X")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IX } };
    verify_lock_and_commit (&lt, p, LOCK_ROOT, (union lt_lock_data){ 0 }, LM_X, parents, 1, &e);
  }

  // LOCK_FSTMBST
  TEST_CASE ("FSTMBST LM_IS")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_ROOT, { 0 }, LM_IS },
      { LOCK_DB, { 0 }, LM_IS }
    };
    verify_lock_and_commit (&lt, p, LOCK_FSTMBST, (union lt_lock_data){ 0 }, LM_IS, parents, 2, &e);
  }

  TEST_CASE ("FSTMBST LM_IX")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_ROOT, { 0 }, LM_IX },
      { LOCK_DB, { 0 }, LM_IX }
    };
    verify_lock_and_commit (&lt, p, LOCK_FSTMBST, (union lt_lock_data){ 0 }, LM_IX, parents, 2, &e);
  }

  TEST_CASE ("FSTMBST LM_S")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_ROOT, { 0 }, LM_IS },
      { LOCK_DB, { 0 }, LM_IS }
    };
    verify_lock_and_commit (&lt, p, LOCK_FSTMBST, (union lt_lock_data){ 0 }, LM_S, parents, 2, &e);
  }

  TEST_CASE ("FSTMBST LM_SIX")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_ROOT, { 0 }, LM_IX },
      { LOCK_DB, { 0 }, LM_IX }
    };
    verify_lock_and_commit (&lt, p, LOCK_FSTMBST, (union lt_lock_data){ 0 }, LM_SIX, parents, 2, &e);
  }

  TEST_CASE ("FSTMBST LM_X")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_ROOT, { 0 }, LM_IX },
      { LOCK_DB, { 0 }, LM_IX }
    };
    verify_lock_and_commit (&lt, p, LOCK_FSTMBST, (union lt_lock_data){ 0 }, LM_X, parents, 2, &e);
  }

  // LOCK_MSLSN
  TEST_CASE ("MSLSN LM_IS")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_ROOT, { 0 }, LM_IS },
      { LOCK_DB, { 0 }, LM_IS }
    };
    verify_lock_and_commit (&lt, p, LOCK_MSLSN, (union lt_lock_data){ 0 }, LM_IS, parents, 2, &e);
  }

  TEST_CASE ("MSLSN LM_IX")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_ROOT, { 0 }, LM_IX },
      { LOCK_DB, { 0 }, LM_IX }
    };
    verify_lock_and_commit (&lt, p, LOCK_MSLSN, (union lt_lock_data){ 0 }, LM_IX, parents, 2, &e);
  }

  TEST_CASE ("MSLSN LM_S")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_ROOT, { 0 }, LM_IS },
      { LOCK_DB, { 0 }, LM_IS }
    };
    verify_lock_and_commit (&lt, p, LOCK_MSLSN, (union lt_lock_data){ 0 }, LM_S, parents, 2, &e);
  }

  TEST_CASE ("MSLSN LM_SIX")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_ROOT, { 0 }, LM_IX },
      { LOCK_DB, { 0 }, LM_IX }
    };
    verify_lock_and_commit (&lt, p, LOCK_MSLSN, (union lt_lock_data){ 0 }, LM_SIX, parents, 2, &e);
  }

  TEST_CASE ("MSLSN LM_X")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_ROOT, { 0 }, LM_IX },
      { LOCK_DB, { 0 }, LM_IX }
    };
    verify_lock_and_commit (&lt, p, LOCK_MSLSN, (union lt_lock_data){ 0 }, LM_X, parents, 2, &e);
  }

  // LOCK_VHP
  TEST_CASE ("VHP LM_IS")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IS } };
    verify_lock_and_commit (&lt, p, LOCK_VHP, (union lt_lock_data){ 0 }, LM_IS, parents, 1, &e);
  }

  TEST_CASE ("VHP LM_IX")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IX } };
    verify_lock_and_commit (&lt, p, LOCK_VHP, (union lt_lock_data){ 0 }, LM_IX, parents, 1, &e);
  }

  TEST_CASE ("VHP LM_S")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IS } };
    verify_lock_and_commit (&lt, p, LOCK_VHP, (union lt_lock_data){ 0 }, LM_S, parents, 1, &e);
  }

  TEST_CASE ("VHP LM_SIX")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IX } };
    verify_lock_and_commit (&lt, p, LOCK_VHP, (union lt_lock_data){ 0 }, LM_SIX, parents, 1, &e);
  }

  TEST_CASE ("VHP LM_X")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IX } };
    verify_lock_and_commit (&lt, p, LOCK_VHP, (union lt_lock_data){ 0 }, LM_X, parents, 1, &e);
  }

  // LOCK_VHPOS
  TEST_CASE ("VHPOS LM_IS")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_VHP, { .var_root = 5 }, LM_IS },
      { LOCK_DB, { 0 }, LM_IS }
    };
    verify_lock_and_commit (&lt, p, LOCK_VHPOS, (union lt_lock_data){ .vhpos = 5 }, LM_IS, parents, 2, &e);
  }

  TEST_CASE ("VHPOS LM_IX")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_VHP, { 0 }, LM_IX },
      { LOCK_DB, { 0 }, LM_IX }
    };
    verify_lock_and_commit (&lt, p, LOCK_VHPOS, (union lt_lock_data){ .vhpos = 5 }, LM_IX, parents, 2, &e);
  }

  TEST_CASE ("VHPOS LM_S")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_VHP, { 0 }, LM_IS },
      { LOCK_DB, { 0 }, LM_IS }
    };
    verify_lock_and_commit (&lt, p, LOCK_VHPOS, (union lt_lock_data){ .vhpos = 5 }, LM_S, parents, 2, &e);
  }

  TEST_CASE ("VHPOS LM_SIX")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_VHP, { 0 }, LM_IX },
      { LOCK_DB, { 0 }, LM_IX }
    };
    verify_lock_and_commit (&lt, p, LOCK_VHPOS, (union lt_lock_data){ .vhpos = 5 }, LM_SIX, parents, 2, &e);
  }

  TEST_CASE ("VHPOS LM_X")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_VHP, { 0 }, LM_IX },
      { LOCK_DB, { 0 }, LM_IX }
    };
    verify_lock_and_commit (&lt, p, LOCK_VHPOS, (union lt_lock_data){ .vhpos = 5 }, LM_X, parents, 2, &e);
  }

  // LOCK_VAR
  TEST_CASE ("VAR LM_IS")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IS } };
    verify_lock_and_commit (&lt, p, LOCK_VAR, (union lt_lock_data){ .var_root = 42 }, LM_IS, parents, 1, &e);
  }

  TEST_CASE ("VAR LM_IX")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IX } };
    verify_lock_and_commit (&lt, p, LOCK_VAR, (union lt_lock_data){ .var_root = 42 }, LM_IX, parents, 1, &e);
  }

  TEST_CASE ("VAR LM_S")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IS } };
    verify_lock_and_commit (&lt, p, LOCK_VAR, (union lt_lock_data){ .var_root = 42 }, LM_S, parents, 1, &e);
  }

  TEST_CASE ("VAR LM_SIX")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IX } };
    verify_lock_and_commit (&lt, p, LOCK_VAR, (union lt_lock_data){ .var_root = 42 }, LM_SIX, parents, 1, &e);
  }

  TEST_CASE ("VAR LM_X")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IX } };
    verify_lock_and_commit (&lt, p, LOCK_VAR, (union lt_lock_data){ .var_root = 42 }, LM_X, parents, 1, &e);
  }

  // LOCK_VAR_NEXT
  TEST_CASE ("VAR_NEXT LM_IS")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_VAR, { .var_root = 42 }, LM_IS },
      { LOCK_DB, { 0 }, LM_IS }
    };
    verify_lock_and_commit (&lt, p, LOCK_VAR_NEXT, (union lt_lock_data){ .var_root_next = 42 }, LM_IS, parents, 2, &e);
  }

  TEST_CASE ("VAR_NEXT LM_IX")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_VAR, { .var_root = 42 }, LM_IX },
      { LOCK_DB, { 0 }, LM_IX }
    };
    verify_lock_and_commit (&lt, p, LOCK_VAR_NEXT, (union lt_lock_data){ .var_root_next = 42 }, LM_IX, parents, 2, &e);
  }

  TEST_CASE ("VAR_NEXT LM_S")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_VAR, { .var_root = 42 }, LM_IS },
      { LOCK_DB, { 0 }, LM_IS }
    };
    verify_lock_and_commit (&lt, p, LOCK_VAR_NEXT, (union lt_lock_data){ .var_root_next = 42 }, LM_S, parents, 2, &e);
  }

  TEST_CASE ("VAR_NEXT LM_SIX")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_VAR, { .var_root = 42 }, LM_IX },
      { LOCK_DB, { 0 }, LM_IX }
    };
    verify_lock_and_commit (&lt, p, LOCK_VAR_NEXT, (union lt_lock_data){ .var_root_next = 42 }, LM_SIX, parents, 2, &e);
  }

  TEST_CASE ("VAR_NEXT LM_X")
  {
    struct lt_lock_parent parents[] = {
      { LOCK_VAR, { .var_root = 42 }, LM_IX },
      { LOCK_DB, { 0 }, LM_IX }
    };
    verify_lock_and_commit (&lt, p, LOCK_VAR_NEXT, (union lt_lock_data){ .var_root_next = 42 }, LM_X, parents, 2, &e);
  }

  // LOCK_RPTREE
  TEST_CASE ("RPTREE LM_IS")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IS } };
    verify_lock_and_commit (&lt, p, LOCK_RPTREE, (union lt_lock_data){ .rptree_root = 100 }, LM_IS, parents, 1, &e);
  }

  TEST_CASE ("RPTREE LM_IX")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IX } };
    verify_lock_and_commit (&lt, p, LOCK_RPTREE, (union lt_lock_data){ .rptree_root = 100 }, LM_IX, parents, 1, &e);
  }

  TEST_CASE ("RPTREE LM_S")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IS } };
    verify_lock_and_commit (&lt, p, LOCK_RPTREE, (union lt_lock_data){ .rptree_root = 100 }, LM_S, parents, 1, &e);
  }

  TEST_CASE ("RPTREE LM_SIX")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IX } };
    verify_lock_and_commit (&lt, p, LOCK_RPTREE, (union lt_lock_data){ .rptree_root = 100 }, LM_SIX, parents, 1, &e);
  }

  TEST_CASE ("RPTREE LM_X")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IX } };
    verify_lock_and_commit (&lt, p, LOCK_RPTREE, (union lt_lock_data){ .rptree_root = 100 }, LM_X, parents, 1, &e);
  }

  // LOCK_TMBST
  TEST_CASE ("TMBST LM_IS")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IS } };
    verify_lock_and_commit (&lt, p, LOCK_TMBST, (union lt_lock_data){ .tmbst_pg = 200 }, LM_IS, parents, 1, &e);
  }

  TEST_CASE ("TMBST LM_IX")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IX } };
    verify_lock_and_commit (&lt, p, LOCK_TMBST, (union lt_lock_data){ .tmbst_pg = 200 }, LM_IX, parents, 1, &e);
  }

  TEST_CASE ("TMBST LM_S")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IS } };
    verify_lock_and_commit (&lt, p, LOCK_TMBST, (union lt_lock_data){ .tmbst_pg = 200 }, LM_S, parents, 1, &e);
  }

  TEST_CASE ("TMBST LM_SIX")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IX } };
    verify_lock_and_commit (&lt, p, LOCK_TMBST, (union lt_lock_data){ .tmbst_pg = 200 }, LM_SIX, parents, 1, &e);
  }

  TEST_CASE ("TMBST LM_X")
  {
    struct lt_lock_parent parents[] = { { LOCK_DB, { 0 }, LM_IX } };
    verify_lock_and_commit (&lt, p, LOCK_TMBST, (union lt_lock_data){ .tmbst_pg = 200 }, LM_X, parents, 1, &e);
  }
}
*/

struct test_case
{
  struct lockt *lt;

  int counter;

  struct lt_lock key1;

  struct pager *p;
};

#include <unistd.h>

static void *
writer_thread_locks_type1_x (void *args)
{
  struct test_case *c = args;
  error e = error_create ();

  for (int i = 0; i < 100; i++)
    {
      // BEGIN TXN
      struct txn tx;
      err_t_panic (pgr_begin_txn (&tx, c->p, &e), &e);

      // X(counter)
      err_t_panic (lockt_lock (c->lt, c->key1, LM_X, &tx, &e), &e);

      // WRITE(counter)
      {
        int counter = c->counter;
        counter++;
        c->counter = counter;
      }

      // COMMIT
      err_t_panic (pgr_commit (c->p, &tx, &e), &e);
    }

  return NULL;
}

TEST (TT_UNIT, lock_table_exclusivity)
{
  error e = error_create ();

  test_err_t_wrap (i_remove_quiet ("test.db", &e), &e);
  test_err_t_wrap (i_remove_quiet ("test.wal", &e), &e);

  struct lockt lt;
  struct thread_pool *tp = tp_open (&e);
  test_err_t_wrap (lockt_init (&lt, &e), &e);
  struct pager *p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_fail_if_null (p);

  struct test_case c = {
    .lt = &lt,

    .counter = 0,

    .key1 = {
        .type = LOCK_DB,
        .data = { 0 },
    },

    .p = p,
  };

  i_thread threads[20];
  for (u32 i = 0; i < arrlen (threads); ++i)
    {
      test_err_t_wrap (i_thread_create (&threads[i], writer_thread_locks_type1_x, &c, &e), &e);
    }
  for (u32 i = 0; i < arrlen (threads); ++i)
    {
      test_err_t_wrap (i_thread_join (&threads[i], &e), &e);
    }

  test_assert_int_equal (c.counter, 100 * arrlen (threads));

  test_err_t_wrap (pgr_close (p, &e), &e);
  lockt_destroy (&lt);
  test_err_t_wrap (tp_free (tp, &e), &e);
}

#endif
