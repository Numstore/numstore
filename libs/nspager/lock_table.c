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

#include "numstore/core/threadpool.h"
#include "numstore/intf/os/file_system.h"
#include "numstore/pager.h"
#include "numstore/test/testing_test.h"
#include <numstore/pager/lock_table.h>

#include <numstore/core/adptv_hash_table.h>
#include <numstore/core/assert.h>
#include <numstore/core/clock_allocator.h>
#include <numstore/core/error.h>
#include <numstore/core/gr_lock.h>
#include <numstore/core/hash_table.h>
#include <numstore/core/hashing.h>
#include <numstore/core/ht_models.h>
#include <numstore/pager/lt_lock.h>
#include <numstore/pager/txn.h>

static void
lt_lock_init_key_from_txn (struct lt_lock *dest)
{
  ASSERT (dest);

  char hcode[sizeof (dest->data) + sizeof (u8)];
  hcode[0] = dest->type;
  u32 hcodelen = 1;

  switch (dest->type)
    {
    case LOCK_DB:
    case LOCK_ROOT:
    case LOCK_FSTMBST:
    case LOCK_MSLSN:
    case LOCK_VHP:
    case LOCK_VHPOS:
      {
        hcodelen += i_memcpy (&hcode[hcodelen], &dest->data.vhpos, sizeof (dest->data.vhpos));
        break;
      }
    case LOCK_VAR:
      {
        hcodelen += i_memcpy (&hcode[hcodelen], &dest->data.vhpos, sizeof (dest->data.vhpos));
        break;
      }
    case LOCK_VAR_NEXT:
      {
        hcodelen += i_memcpy (&hcode[hcodelen], &dest->data.var_root_next, sizeof (dest->data.var_root_next));
        break;
      }
    case LOCK_RPTREE:
      {
        hcodelen += i_memcpy (&hcode[hcodelen], &dest->data.rptree_root, sizeof (dest->data.rptree_root));
        break;
      }
    case LOCK_TMBST:
      {
        hcodelen += i_memcpy (&hcode[hcodelen], &dest->data.tmbst_pg, sizeof (dest->data.tmbst_pg));
        break;
      }
    }

  dest->type = dest->type;
  dest->data = dest->data;

  struct cstring lock_type_hcode = {
    .data = hcode,
    .len = hcodelen,
  };

  hnode_init (&dest->lock_type_node, fnv1a_hash (lock_type_hcode));
}

static void
lt_lock_init_key (struct lt_lock *dest, enum lt_lock_type type, union lt_lock_data data)
{
  ASSERT (dest);
  dest->type = type;
  dest->data = data;
  lt_lock_init_key_from_txn (dest);
}

static bool
lt_lock_eq (const struct hnode *left, const struct hnode *right)
{
  const struct lt_lock *_left = container_of (left, struct lt_lock, lock_type_node);
  const struct lt_lock *_right = container_of (right, struct lt_lock, lock_type_node);

  if (_left->type != _right->type)
    {
      return false;
    }

  switch (_left->type)
    {
    case LOCK_DB:
      {
        return true;
      }
    case LOCK_ROOT:
      {
        return true;
      }
    case LOCK_FSTMBST:
      {
        return true;
      }
    case LOCK_MSLSN:
      {
        return true;
      }
    case LOCK_VHP:
      {
        return true;
      }
    case LOCK_VHPOS:
      {
        return _left->data.vhpos == _right->data.vhpos;
      }
    case LOCK_VAR:
      {
        return _left->data.var_root == _right->data.var_root;
      }
    case LOCK_VAR_NEXT:
      {
        return _left->data.var_root_next == _right->data.var_root_next;
      }
    case LOCK_RPTREE:
      {
        return _left->data.rptree_root == _right->data.rptree_root;
      }
    case LOCK_TMBST:
      {
        return _left->data.tmbst_pg == _right->data.tmbst_pg;
      }
    }
  UNREACHABLE ();
}

err_t
lockt_init (struct lockt *t, error *e)
{
  if (clck_alloc_open (&t->gr_lock_alloc, sizeof (struct gr_lock), 1000, e))
    {
      return e->cause_code;
    }

  struct adptv_htable_settings settings = {
    .max_load_factor = 8,
    .min_load_factor = 1,
    .rehashing_work = 28,
    .max_size = 2048,
    .min_size = 10,
  };

  if (adptv_htable_init (&t->table, settings, e))
    {
      clck_alloc_close (&t->gr_lock_alloc);
      return e->cause_code;
    }

  return SUCCESS;
}

void
lockt_destroy (struct lockt *t)
{
  // TODO - wait for all locks?
  clck_alloc_close (&t->gr_lock_alloc);
  adptv_htable_free (&t->table);
}

static struct lt_lock *
lockt_lock_once (
    struct lockt *t,
    enum lt_lock_type type,
    union lt_lock_data data,
    enum lock_mode mode,
    struct txn *tx,
    error *e)
{
  // Fetch this lock type
  struct lt_lock key;
  lt_lock_init_key (&key, type, data);
  struct hnode *node = adptv_htable_lookup (&t->table, &key.lock_type_node, lt_lock_eq);

  // FOUND
  if (node != NULL)
    {
      struct lt_lock *existing = container_of (node, struct lt_lock, lock_type_node);

      // If this is the same lock to the same tx, do nothing
      if (existing->tid == tx->tid)
        {
          ASSERTF (
              existing->mode == mode,
              "Only duplicate or smaller modes are allowed on multiple locks in a txn, "
              "otherwise call upgrade");
          return existing;
        }

      // Create a new lock wrapper to this txn
      struct lt_lock *lock = txn_newlock (tx, type, data, mode, e);
      if (lock == NULL)
        {
          return NULL;
        }

      // LOCK
      struct gr_lock *_gr_lock = existing->lock;
      if (gr_lock (_gr_lock, mode, e))
        {
          txn_freelock (tx, lock);
          return NULL;
        }

      lock->lock = _gr_lock;

      // INSERT
      if (adptv_htable_insert (&t->table, &lock->lock_type_node, e))
        {
          gr_unlock (_gr_lock, mode);
          clck_alloc_free (&t->gr_lock_alloc, _gr_lock);
          txn_freelock (tx, lock);
          return NULL;
        }

      return lock;
    }

  // NOT FOUND
  else
    {
      // Create a new lock wrapper to this txn
      struct lt_lock *lock = txn_newlock (tx, type, data, mode, e);
      if (lock == NULL)
        {
          return NULL;
        }

      // ALLOC
      struct gr_lock *_gr_lock = clck_alloc_alloc (&t->gr_lock_alloc, e);
      if (_gr_lock == NULL)
        {
          txn_freelock (tx, lock);
          return NULL;
        }

      // INIT
      if (gr_lock_init (_gr_lock, e))
        {
          clck_alloc_free (&t->gr_lock_alloc, _gr_lock);
          txn_freelock (tx, lock);
          return NULL;
        }

      // LOCK
      if (gr_lock (_gr_lock, mode, e))
        {
          gr_lock_destroy (_gr_lock);
          clck_alloc_free (&t->gr_lock_alloc, _gr_lock);
          txn_freelock (tx, lock);
          return NULL;
        }

      lock->lock = _gr_lock;

      // INSERT
      if (adptv_htable_insert (&t->table, &lock->lock_type_node, e))
        {
          gr_unlock (_gr_lock, mode);
          clck_alloc_free (&t->gr_lock_alloc, _gr_lock);
          txn_freelock (tx, lock);
          return NULL;
        }

      return lock;
    }
}

static const int parent_lock[] = {
  [LOCK_DB] = -1,
  [LOCK_ROOT] = LOCK_DB,
  [LOCK_FSTMBST] = LOCK_ROOT,
  [LOCK_MSLSN] = LOCK_ROOT,
  [LOCK_VHP] = LOCK_DB,
  [LOCK_VHPOS] = LOCK_VHP,
  [LOCK_VAR] = LOCK_DB,
  [LOCK_VAR_NEXT] = LOCK_VAR,
  [LOCK_RPTREE] = LOCK_DB,
  [LOCK_TMBST] = LOCK_DB,
};

static inline enum lock_mode
get_parent_mode (enum lock_mode child_mode)
{
  switch (child_mode)
    {
    case LM_IS:
    case LM_S:
      {
        return LM_IS;
      }
    case LM_IX:
    case LM_SIX:
    case LM_X:
      {
        return LM_IX;
      }
    case LM_COUNT:
      {
        UNREACHABLE ();
      }
    }
  UNREACHABLE ();
}

struct lt_lock *
lockt_lock (
    struct lockt *t,
    enum lt_lock_type type,
    union lt_lock_data data,
    enum lock_mode mode,
    struct txn *tx,
    error *e)
{
  // First you need to obtain a lock on the parent
  int ptype = parent_lock[type];

  if (ptype != -1)
    {
      enum lock_mode pmode = get_parent_mode (mode);

      struct lt_lock *plock = lockt_lock (t, ptype, data, pmode, tx, e);

      if (plock == NULL)
        {
          return NULL;
        }
    }

  // Then lock this node
  return lockt_lock_once (t, type, data, mode, tx, e);
}

#ifndef NTEST

struct lt_lock_parent
{
  enum lt_lock_type type;
  union lt_lock_data data;
  enum lock_mode mode;
};

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
    node = adptv_htable_lookup (&lt->table, &key.lock_type_node, lt_lock_eq);
    test_fail_if_null (node);
    lock = container_of (node, struct lt_lock, lock_type_node);
    test_assert_int_equal (lock->mode, mode);

    // Parent locks
    for (size_t i = 0; i < num_parents; i++)
      {
        lt_lock_init_key (&key, expected_parents[i].type, expected_parents[i].data);
        node = adptv_htable_lookup (&lt->table, &key.lock_type_node, lt_lock_eq);
        test_fail_if_null (node);
        lock = container_of (node, struct lt_lock, lock_type_node);
        test_assert_int_equal (lock->mode, expected_parents[i].mode);
      }

    // Verify ONLY these locks
    test_assert_int_equal (adptv_htable_size (&lt->table), num_parents + 1);
  }

  test_err_t_wrap (pgr_commit (p, &tx, e), e);

  {
    // This lock
    lt_lock_init_key (&key, lock_type, lock_data);
    node = adptv_htable_lookup (&lt->table, &key.lock_type_node, lt_lock_eq);
    test_assert_equal (node, NULL);

    // Parent locks
    for (size_t i = 0; i < num_parents; i++)
      {
        lt_lock_init_key (&key, expected_parents[i].type, expected_parents[i].data);
        node = adptv_htable_lookup (&lt->table, &key.lock_type_node, lt_lock_eq);
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
      { LOCK_VHP, { 0 }, LM_IS },
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

#endif

err_t
lockt_upgrade (struct lockt *t, struct lt_lock *lock, enum lock_mode new_mode, error *e)
{
  if (new_mode <= lock->mode)
    {
      return SUCCESS;
    }

  int ptype = parent_lock[lock->type];

  if (ptype != -1)
    {
      enum lock_mode new_parent_mode = get_parent_mode (new_mode);
      enum lock_mode old_parent_mode = get_parent_mode (lock->mode);

      // UPGRADE PARENT
      if (new_parent_mode > old_parent_mode)
        {
          struct lt_lock key;
          lt_lock_init_key (&key, ptype, lock->data);

          // FIND PARENT
          struct hnode *_found = adptv_htable_lookup (&t->table, &key.lock_type_node, lt_lock_eq);
          ASSERT (_found);
          struct lt_lock *found = container_of (_found, struct lt_lock, lock_type_node);

          // UPGRADE
          err_t_wrap (lockt_upgrade (t, found, new_parent_mode, e), e);
        }
    }

  if (gr_upgrade (lock->lock, lock->mode, new_mode, e))
    {
      return e->cause_code;
    }

  lock->mode = new_mode;

  return SUCCESS;
}

err_t
lockt_unlock (struct lockt *t, struct txn *tx, error *e)
{
  ASSERT (t);
  ASSERT (tx);

  struct lt_lock *curr = tx->locks;

  while (curr != NULL)
    {
      struct lt_lock *next = curr->next;

      // REMOVE
      err_t_wrap (adptv_htable_delete (NULL, &t->table, &curr->lock_type_node, lt_lock_eq, e), e);

      // UNLOCK
      struct gr_lock *gr_lock_ptr = curr->lock;
      bool is_free = gr_unlock (gr_lock_ptr, curr->mode);

      // DELETE GR_LOCK IF LAST
      if (is_free)
        {
          gr_lock_destroy (gr_lock_ptr);
          clck_alloc_free (&t->gr_lock_alloc, gr_lock_ptr);
        }

      curr->lock = NULL;
      curr = next;
    }

  // FREE LOCKS
  txn_free_all_locks (tx);

  return SUCCESS;
}

static void
consume_node (struct hnode *node, void *ctx)
{
  int *log_level = ctx;
  struct lt_lock *lock = container_of (node, struct lt_lock, lock_type_node);
  i_print_lt_lock (*log_level, lock);
}

void
i_log_lockt (int log_level, struct lockt *t)
{
  i_log (log_level, "================== LOCK TABLE START ==================\n");
  adptv_htable_foreach (&t->table, consume_node, &log_level);
  i_log (log_level, "================== LOCK TABLE END ==================\n");
}
