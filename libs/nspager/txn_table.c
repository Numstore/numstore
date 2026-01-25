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
 *   Implements txn_table.h. Transaction table management with concurrency control and lock tracking.
 */

#include <numstore/pager/txn_table.h>

#include <numstore/core/adptv_hash_table.h>
#include <numstore/core/alloc.h>
#include <numstore/core/assert.h>
#include <numstore/core/clock_allocator.h>
#include <numstore/core/dbl_buffer.h>
#include <numstore/core/deserializer.h>
#include <numstore/core/error.h>
#include <numstore/core/hash_table.h>
#include <numstore/core/ht_models.h>
#include <numstore/core/latch.h>
#include <numstore/core/random.h>
#include <numstore/core/serializer.h>
#include <numstore/core/slab_alloc.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/pager/txn.h>
#include <numstore/test/testing.h>

#include <unistd.h>

DEFINE_DBG_ASSERT (
    struct txn_table, txn_table, t, {
      ASSERT (t);
    })

#define TXN_SERIAL_UNIT (sizeof (txid) + sizeof (lsn) + sizeof (lsn))

//////////////////////////////////////////////////
///// Table

err_t
txnt_open (struct txn_table *dest, error *e)
{
  struct adptv_htable_settings settings = {
    .max_load_factor = 8,
    .min_load_factor = 1,
    .rehashing_work = 28,
    .max_size = 2048,
    .min_size = 10,
  };

  err_t_wrap (adptv_htable_init (&dest->t, settings, e), e);
  latch_init (&dest->l);

  return SUCCESS;
}

void
txnt_close (struct txn_table *t)
{
  DBG_ASSERT (txn_table, t);
  latch_lock (&t->l);

  adptv_htable_free (&t->t);

  latch_unlock (&t->l);
}

#ifndef NTEST
TEST (TT_UNIT, txnt_open_close_basic)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);
  txnt_close (&t);
}

TEST (TT_UNIT, txnt_open_failures_are_reported)
{
  error e = error_create ();
  for (int i = 0; i < 4; ++i)
    {
      struct txn_table t;
      test_err_t_wrap (txnt_open (&t, &e), &e);
      txnt_close (&t);
    }
}
#endif

static inline const char *
txn_state_to_str (int state)
{
  switch (state)
    {
      case_ENUM_RETURN_STRING (TX_RUNNING);
      case_ENUM_RETURN_STRING (TX_CANDIDATE_FOR_UNDO);
      case_ENUM_RETURN_STRING (TX_COMMITTED);
    }

  UNREACHABLE ();
}

static void
i_log_txn_in_txnt (struct hnode *node, void *_log_level)
{
  int *log_level = _log_level;
  struct txn *tx = container_of (node, struct txn, node);

  latch_lock (&tx->l);

  i_printf (
      *log_level,
      "| %d | %" PRtxid " | %" PRpgno " | %" PRpgno " | %s |\n",
      tx->node.hcode, tx->tid, tx->data.last_lsn, tx->data.undo_next_lsn, txn_state_to_str (tx->data.state));

  latch_unlock (&tx->l);
}

void
i_log_txnt (int log_level, struct txn_table *t)
{
  latch_lock (&t->l);

  i_log (log_level, "============ TXN TABLE START ===============\n");
  adptv_htable_foreach (&t->t, i_log_txn_in_txnt, &log_level);
  i_log (log_level, "============ TXN TABLE END   ===============\n");

  latch_unlock (&t->l);
}

struct merge_ctx
{
  struct txn_table *dest;
  error *e;
  struct dbl_buffer *txn_dest;
  struct slab_alloc *alloc;
};

static void
merge_txn (struct txn *tx, void *vctx)
{
  struct merge_ctx *ctx = vctx;
  ASSERT (ctx->txn_dest == NULL || ctx->txn_dest->size == sizeof (struct txn));

  if (ctx->e->cause_code)
    {
      return;
    }

  latch_lock (&tx->l);

  if (txn_exists (ctx->dest, tx->tid))
    {
      goto theend;
    }

  struct txn *target_txn = tx;

  // If provided, allocate a new transaction to copy over
  if (ctx->txn_dest && ctx->alloc)
    {
      target_txn = slab_alloc_alloc (ctx->alloc, ctx->e);
      if (target_txn == NULL)
        {
          goto theend;
        }
      if (dblb_append (ctx->txn_dest, &target_txn, 1, ctx->e))
        {
          goto theend;
        }
      txn_init (target_txn, tx->tid, tx->data);
    }

  // Insert into the table
  if (txnt_insert_txn (ctx->dest, target_txn, ctx->e))
    {
      goto theend;
    }

theend:

  latch_unlock (&tx->l);
}

err_t
txnt_merge_into (
    struct txn_table *dest,
    struct txn_table *src,
    struct dbl_buffer *txn_dest,
    struct slab_alloc *alloc,
    error *e)
{
  struct merge_ctx ctx = {
    .dest = dest,
    .e = e,
    .txn_dest = txn_dest,
  };

  latch_lock (&src->l);
  txnt_foreach (src, merge_txn, &ctx);
  latch_unlock (&src->l);

  return ctx.e->cause_code;
}

#ifndef NTEST
TEST (TT_UNIT, txnt_merge_into_empty_to_empty)
{
  error e = error_create ();
  struct txn_table dest, src;
  test_err_t_wrap (txnt_open (&dest, &e), &e);
  test_err_t_wrap (txnt_open (&src, &e), &e);

  err_t result = txnt_merge_into (&dest, &src, NULL, NULL, &e);
  test_assert (result == SUCCESS);

  txnt_close (&dest);
  txnt_close (&src);
}

TEST (TT_UNIT, txnt_merge_into_with_data)
{
  error e = error_create ();
  struct txn_table dest, src;
  test_err_t_wrap (txnt_open (&dest, &e), &e);
  test_err_t_wrap (txnt_open (&src, &e), &e);

  // Add to dest
  struct txn dest_txns[5];
  for (int i = 0; i < 5; i++)
    {
      txn_init (&dest_txns[i], i + 1, (struct txn_data){
                                          .last_lsn = (i + 1) * 10,
                                          .undo_next_lsn = (i + 1) * 10 - 1,
                                          .state = TX_RUNNING,
                                      });
      err_t result = txnt_insert_txn (&dest, &dest_txns[i], &e);
      test_assert (result == SUCCESS);
    }

  // Add to src (different tids)
  struct txn src_txns[5];
  for (int i = 0; i < 5; i++)
    {
      txn_init (&src_txns[i], i + 6, (struct txn_data){
                                         .last_lsn = (i + 6) * 10,
                                         .undo_next_lsn = (i + 6) * 10 - 1,
                                         .state = TX_CANDIDATE_FOR_UNDO,
                                     });
      err_t result = txnt_insert_txn (&src, &src_txns[i], &e);
      test_assert (result == SUCCESS);
    }

  err_t result = txnt_merge_into (&dest, &src, NULL, NULL, &e);
  test_assert (result == SUCCESS);

  // Verify all exist in dest
  for (txid tid = 1; tid <= 10; tid++)
    {
      test_assert (txn_exists (&dest, tid));
    }

  txnt_close (&dest);
  txnt_close (&src);
}

TEST (TT_UNIT, txnt_merge_into_no_duplicate_insert)
{
  error e = error_create ();
  struct txn_table dest, src;
  test_err_t_wrap (txnt_open (&dest, &e), &e);
  test_err_t_wrap (txnt_open (&src, &e), &e);

  // Add same tid to both with different values
  struct txn dest_tx;
  txn_init (&dest_tx, 42, (struct txn_data){
                              .last_lsn = 100,
                              .undo_next_lsn = 90,
                              .state = TX_RUNNING,
                          });
  err_t result = txnt_insert_txn (&dest, &dest_tx, &e);
  test_assert (result == SUCCESS);

  struct txn src_tx;
  txn_init (&src_tx, 42, (struct txn_data){
                             .last_lsn = 200,
                             .undo_next_lsn = 190,
                             .state = TX_COMMITTED,
                         });
  result = txnt_insert_txn (&src, &src_tx, &e);
  test_assert (result == SUCCESS);

  result = txnt_merge_into (&dest, &src, NULL, NULL, &e);
  test_assert (result == SUCCESS);

  // Dest should keep its original value
  struct txn *retrieved;
  bool found = txnt_get (&retrieved, &dest, 42);
  test_assert (found);
  test_assert (retrieved->data.last_lsn == 100);
  test_assert (retrieved->data.state == TX_RUNNING);

  txnt_close (&dest);
  txnt_close (&src);
}
#endif

static void
find_max_undo (struct txn *tx, void *vctx)
{
  slsn *max = vctx;

  latch_lock (&tx->l);

  if (tx->data.state == TX_CANDIDATE_FOR_UNDO)
    {
      if ((slsn)tx->data.undo_next_lsn > *max)
        {
          *max = tx->data.undo_next_lsn;
        }
    }

  latch_unlock (&tx->l);
}

slsn
txnt_max_u_undo_lsn (struct txn_table *t)
{
  slsn max = -1;

  latch_lock (&t->l);
  txnt_foreach (t, find_max_undo, &max);
  latch_unlock (&t->l);

  return max;
}

#ifndef NTEST
TEST (TT_UNIT, txnt_max_u_undo_lsn_empty)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  slsn max = txnt_max_u_undo_lsn (&t);
  test_assert (max == -1);

  txnt_close (&t);
}

TEST (TT_UNIT, txnt_max_u_undo_lsn_with_candidates)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  struct txn tx1, tx2, tx3, tx4;

  // Running - should be ignored
  txn_init (&tx1, 1, (struct txn_data){ .last_lsn = 100, .undo_next_lsn = 100, .state = TX_RUNNING });

  // Candidates
  txn_init (&tx2, 2, (struct txn_data){ .last_lsn = 50, .undo_next_lsn = 40, .state = TX_CANDIDATE_FOR_UNDO });
  txn_init (&tx3, 3, (struct txn_data){ .last_lsn = 80, .undo_next_lsn = 75, .state = TX_CANDIDATE_FOR_UNDO });
  txn_init (&tx4, 4, (struct txn_data){ .last_lsn = 60, .undo_next_lsn = 55, .state = TX_CANDIDATE_FOR_UNDO });

  txnt_insert_txn (&t, &tx1, &e);
  txnt_insert_txn (&t, &tx2, &e);
  txnt_insert_txn (&t, &tx3, &e);
  txnt_insert_txn (&t, &tx4, &e);

  slsn max = txnt_max_u_undo_lsn (&t);
  test_assert (max == 75);

  txnt_close (&t);
}

TEST (TT_UNIT, txnt_max_u_undo_lsn_only_running_txns)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  struct txn tx1, tx2, tx3;
  txn_init (&tx1, 1, (struct txn_data){ .last_lsn = 100, .undo_next_lsn = 90, .state = TX_RUNNING });
  txn_init (&tx2, 2, (struct txn_data){ .last_lsn = 200, .undo_next_lsn = 190, .state = TX_RUNNING });
  txn_init (&tx3, 3, (struct txn_data){ .last_lsn = 300, .undo_next_lsn = 290, .state = TX_COMMITTED });

  txnt_insert_txn (&t, &tx1, &e);
  txnt_insert_txn (&t, &tx2, &e);
  txnt_insert_txn (&t, &tx3, &e);

  slsn max = txnt_max_u_undo_lsn (&t);
  test_assert (max == -1);

  txnt_close (&t);
}
#endif

struct foreach_ctx
{
  void (*action) (struct txn *, void *ctx);
  void *ctx;
};

static void
hnode_foreach (struct hnode *node, void *ctx)
{
  struct foreach_ctx *_ctx = ctx;
  _ctx->action (container_of (node, struct txn, node), _ctx->ctx);
}

void
txnt_foreach (struct txn_table *t, void (*action) (struct txn *, void *ctx), void *ctx)
{
  struct foreach_ctx _ctx = {
    .action = action,
    .ctx = ctx,
  };
  adptv_htable_foreach (&t->t, hnode_foreach, &_ctx);
}

u32
txnt_get_size (struct txn_table *dest)
{
  return adptv_htable_size (&dest->t);
}

static bool
txn_equals_for_exists (const struct hnode *left, const struct hnode *right)
{
  // Might have passed the exact same reference as exists in the htable
  if (left == right)
    {
      return true;
    }

  // Otherwise, passed a key with just relevant information
  else
    {
      struct txn *_left = container_of (left, struct txn, node);
      struct txn *_right = container_of (right, struct txn, node);

      latch_lock (&_left->l);
      latch_lock (&_right->l);

      bool ret = _left->tid == _right->tid;

      latch_unlock (&_right->l);
      latch_unlock (&_left->l);

      return ret;
    }
}

bool
txn_exists (struct txn_table *t, txid tid)
{
  struct txn tx;
  txn_key_init (&tx, tid);

  struct hnode *ret = adptv_htable_lookup (&t->t, &tx.node, txn_equals_for_exists);

  return ret != NULL;
}

#ifndef NTEST
TEST (TT_UNIT, txnt_exists)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  test_assert (!txn_exists (&t, 1100));

  struct txn tx;
  txn_init (&tx, 1100, (struct txn_data){
                           .last_lsn = 100,
                           .undo_next_lsn = 90,
                           .state = TX_RUNNING,
                       });

  err_t result = txnt_insert_txn (&t, &tx, &e);
  test_assert (result == SUCCESS);
  test_assert (txn_exists (&t, 1100));

  txnt_close (&t);
}
#endif

err_t
txnt_insert_txn (struct txn_table *t, struct txn *tx, error *e)
{
  DBG_ASSERT (txn_table, t);
  ASSERT (!txn_exists (t, tx->tid));

  latch_lock (&t->l);

  if (adptv_htable_insert (&t->t, &tx->node, e))
    {
      goto theend;
    }

theend:
  latch_unlock (&t->l);
  return e->cause_code;
}

err_t
txnt_insert_txn_if_not_exists (struct txn_table *t, struct txn *tx, error *e)
{
  DBG_ASSERT (txn_table, t);

  latch_lock (&t->l);

  if (txn_exists (t, tx->tid))
    {
      goto theend;
    }

  if (adptv_htable_insert (&t->t, &tx->node, e))
    {
      goto theend;
    }

theend:
  latch_unlock (&t->l);
  return e->cause_code;
}

#ifndef NTEST
TEST (TT_UNIT, txnt_insert_if_not_exists_new)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  struct txn tx;
  txn_init (&tx, 900, (struct txn_data){
                          .last_lsn = 100,
                          .undo_next_lsn = 90,
                          .state = TX_RUNNING,
                      });

  err_t result = txnt_insert_txn_if_not_exists (&t, &tx, &e);
  test_assert (result == SUCCESS);

  struct txn *retrieved;
  bool found = txnt_get (&retrieved, &t, 900);
  test_assert (found);
  test_assert (retrieved->tid == 900);

  txnt_close (&t);
}

TEST (TT_UNIT, txnt_insert_if_not_exists_existing)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  struct txn tx1;
  txn_init (&tx1, 1000, (struct txn_data){
                            .last_lsn = 100,
                            .undo_next_lsn = 90,
                            .state = TX_RUNNING,
                        });

  err_t result = txnt_insert_txn (&t, &tx1, &e);
  test_assert (result == SUCCESS);

  struct txn tx2;
  txn_init (&tx2, 1000, (struct txn_data){
                            .last_lsn = 200,
                            .undo_next_lsn = 180,
                            .state = TX_COMMITTED,
                        });

  result = txnt_insert_txn_if_not_exists (&t, &tx2, &e);
  test_assert (result == SUCCESS);

  struct txn *retrieved;
  bool found = txnt_get (&retrieved, &t, 1000);
  test_assert (found);
  test_assert (retrieved->data.last_lsn == 100);
  test_assert (retrieved->data.state == TX_RUNNING);

  txnt_close (&t);
}

TEST (TT_UNIT, txnt_multiple_transactions_different_states)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  struct txn tx1, tx2, tx3;

  txn_init (&tx1, 1, (struct txn_data){ .last_lsn = 10, .undo_next_lsn = 9, .state = TX_RUNNING });
  txn_init (&tx2, 2, (struct txn_data){ .last_lsn = 20, .undo_next_lsn = 19, .state = TX_CANDIDATE_FOR_UNDO });
  txn_init (&tx3, 3, (struct txn_data){ .last_lsn = 30, .undo_next_lsn = 29, .state = TX_COMMITTED });

  test_assert (txnt_insert_txn (&t, &tx1, &e) == SUCCESS);
  test_assert (txnt_insert_txn (&t, &tx2, &e) == SUCCESS);
  test_assert (txnt_insert_txn (&t, &tx3, &e) == SUCCESS);

  struct txn *retrieved;

  test_assert (txnt_get (&retrieved, &t, 1));
  test_assert (retrieved->data.state == TX_RUNNING);

  test_assert (txnt_get (&retrieved, &t, 2));
  test_assert (retrieved->data.state == TX_CANDIDATE_FOR_UNDO);

  test_assert (txnt_get (&retrieved, &t, 3));
  test_assert (retrieved->data.state == TX_COMMITTED);

  txnt_close (&t);
}
#endif

bool
txnt_get (struct txn **dest, struct txn_table *t, txid tid)
{
  DBG_ASSERT (txn_table, t);

  struct txn key;
  txn_key_init (&key, tid);

  latch_lock (&t->l);

  struct hnode *node = adptv_htable_lookup (&t->t, &key.node, txn_equals_for_exists);
  if (node)
    {
      *dest = container_of (node, struct txn, node);
    }

  latch_unlock (&t->l);

  return node != NULL;
}

void
txnt_get_expect (struct txn **dest, struct txn_table *t, txid tid)
{
  DBG_ASSERT (txn_table, t);

  struct txn key;
  txn_key_init (&key, tid);

  latch_lock (&t->l);

  struct hnode *node = adptv_htable_lookup (&t->t, &key.node, txn_equals_for_exists);
  ASSERT (node);
  *dest = container_of (node, struct txn, node);

  latch_unlock (&t->l);
}

#ifndef NTEST
TEST (TT_UNIT, txnt_get_nonexistent_returns_false)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  struct txn *retrieved;
  bool found = txnt_get (&retrieved, &t, 9999);
  test_assert (!found);

  txnt_close (&t);
}

TEST (TT_UNIT, txnt_insert_and_get_tx_running)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  struct txn tx;
  txn_init (&tx, 100, (struct txn_data){
                          .last_lsn = 50,
                          .undo_next_lsn = 40,
                          .state = TX_RUNNING,
                      });

  err_t result = txnt_insert_txn (&t, &tx, &e);
  test_assert (result == SUCCESS);

  struct txn *retrieved;
  bool found = txnt_get (&retrieved, &t, 100);
  test_assert (found);
  test_assert (txn_data_equal (&retrieved->data, &tx.data));

  txnt_close (&t);
}

TEST (TT_UNIT, txnt_insert_and_get_tx_candidate_for_undo)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  struct txn tx;
  txn_init (&tx, 200, (struct txn_data){
                          .last_lsn = 100,
                          .undo_next_lsn = 90,
                          .state = TX_CANDIDATE_FOR_UNDO,
                      });

  err_t result = txnt_insert_txn (&t, &tx, &e);
  test_assert (result == SUCCESS);

  struct txn *retrieved;
  bool found = txnt_get (&retrieved, &t, 200);
  test_assert (found);
  test_assert (retrieved->tid == 200);
  test_assert (retrieved->data.last_lsn == 100);
  test_assert (retrieved->data.undo_next_lsn == 90);
  test_assert (retrieved->data.state == TX_CANDIDATE_FOR_UNDO);

  txnt_close (&t);
}

TEST (TT_UNIT, txnt_update_last_lsn)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  struct txn tx;
  txn_init (&tx, 600, (struct txn_data){
                          .last_lsn = 100,
                          .undo_next_lsn = 90,
                          .state = TX_RUNNING,
                      });

  txnt_insert_txn (&t, &tx, &e);

  // Fetch, update, verify
  struct txn *retrieved;
  bool found = txnt_get (&retrieved, &t, 600);
  test_assert (found);

  struct txn_data new_data = retrieved->data;
  new_data.last_lsn = 200;
  txn_update_data (retrieved, new_data);

  // Verify update
  found = txnt_get (&retrieved, &t, 600);
  test_assert (found);
  test_assert (retrieved->data.last_lsn == 200);

  txnt_close (&t);
}

TEST (TT_UNIT, txnt_state_transitions_all_types)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  struct txn tx;
  txn_init (&tx, 2000, (struct txn_data){
                           .last_lsn = 100,
                           .undo_next_lsn = 99,
                           .state = TX_RUNNING,
                       });

  txnt_insert_txn (&t, &tx, &e);

  struct txn key;
  struct txn *retrieved = &key;

  test_assert (txnt_get (&retrieved, &t, 2000));
  test_assert (retrieved->data.state == TX_RUNNING);

  // Transition to CANDIDATE_FOR_UNDO
  struct txn_data new_data = retrieved->data;
  new_data.state = TX_CANDIDATE_FOR_UNDO;
  txn_update_data (retrieved, new_data);

  test_assert (txnt_get (&retrieved, &t, 2000));
  test_assert (retrieved->data.state == TX_CANDIDATE_FOR_UNDO);

  // Transition to COMMITTED
  new_data = retrieved->data;
  new_data.state = TX_COMMITTED;
  txn_update_data (retrieved, new_data);

  test_assert (txnt_get (&retrieved, &t, 2000));
  test_assert (retrieved->data.state == TX_COMMITTED);

  txnt_close (&t);
}
#endif

err_t
txnt_remove_txn (bool *exists, struct txn_table *t, struct txn *tx, error *e)
{
  DBG_ASSERT (txn_table, t);

  latch_lock (&t->l);

  struct hnode *node = adptv_htable_lookup (&t->t, &tx->node, txn_equals_for_exists);

  if (node == NULL)
    {
      *exists = false;
      goto theend;
    }

  *exists = true;

  if (adptv_htable_delete (NULL, &t->t, node, txn_equals_for_exists, e))
    {
      goto theend;
    }

theend:
  latch_unlock (&t->l);
  return e->cause_code;
}

err_t
txnt_remove_txn_expect (struct txn_table *t, struct txn *tx, error *e)
{
  DBG_ASSERT (txn_table, t);

  latch_lock (&t->l);

  struct hnode *node = adptv_htable_lookup (&t->t, &tx->node, txn_equals_for_exists);

  ASSERT (node != NULL);

  if (adptv_htable_delete (NULL, &t->t, node, txn_equals_for_exists, e))
    {
      latch_unlock (&t->l);
      return e->cause_code;
    }

  latch_unlock (&t->l);
  return e->cause_code;
}

#ifndef NTEST
TEST (TT_UNIT, txnt_remove_existing_txn)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  struct txn tx;
  txn_init (&tx, 400, (struct txn_data){
                          .last_lsn = 100,
                          .undo_next_lsn = 90,
                          .state = TX_RUNNING,
                      });

  err_t result = txnt_insert_txn (&t, &tx, &e);
  test_assert (result == SUCCESS);

  bool removed;
  result = txnt_remove_txn (&removed, &t, &tx, &e);
  test_assert (result == SUCCESS);
  test_assert (removed);

  struct txn *retrieved;
  bool found = txnt_get (&retrieved, &t, 400);
  test_assert (!found);

  txnt_close (&t);
}

TEST (TT_UNIT, txnt_remove_nonexistent_txn)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  struct txn tx;
  txn_key_init (&tx, 500);

  bool removed;
  err_t result = txnt_remove_txn (&removed, &t, &tx, &e);
  test_assert (result == SUCCESS);
  test_assert (!removed);

  txnt_close (&t);
}

TEST (TT_UNIT, txnt_double_remove_same_transaction)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  struct txn tx;
  txn_init (&tx, 100, (struct txn_data){
                          .last_lsn = 50,
                          .undo_next_lsn = 49,
                          .state = TX_RUNNING,
                      });

  txnt_insert_txn (&t, &tx, &e);

  bool removed;
  txnt_remove_txn (&removed, &t, &tx, &e);
  test_assert (removed);

  txnt_remove_txn (&removed, &t, &tx, &e);
  test_assert (!removed);

  txnt_close (&t);
}

TEST (TT_UNIT, txnt_operations_after_remove)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  struct txn tx;
  txn_init (&tx, 200, (struct txn_data){
                          .last_lsn = 50,
                          .undo_next_lsn = 49,
                          .state = TX_RUNNING,
                      });

  txnt_insert_txn (&t, &tx, &e);

  bool removed;
  txnt_remove_txn (&removed, &t, &tx, &e);
  test_assert (removed);

  // Get should fail
  struct txn *retrieved;
  bool found = txnt_get (&retrieved, &t, 200);
  test_assert (!found);

  txnt_close (&t);
}
#endif

u32
txnt_get_serialize_size (struct txn_table *t)
{
  return adptv_htable_size (&t->t) * TXN_SERIAL_UNIT;
}

struct txn_serialize_ctx
{
  struct serializer s;
};

static void
hnode_foreach_serialize (struct hnode *node, void *ctx)
{
  struct txn_serialize_ctx *_ctx = ctx;

  struct txn *tx = container_of (node, struct txn, node);

  txid tid;
  lsn last_lsn;
  lsn undo_next_lsn;

  latch_lock (&tx->l);

  tid = tx->tid;
  last_lsn = tx->data.last_lsn;
  undo_next_lsn = tx->data.undo_next_lsn;

  latch_unlock (&tx->l);

  srlizr_write_expect (&_ctx->s, &tid, sizeof (tid));
  srlizr_write_expect (&_ctx->s, &last_lsn, sizeof (last_lsn));
  srlizr_write_expect (&_ctx->s, &undo_next_lsn, sizeof (undo_next_lsn));
}

u32
txnt_serialize (u8 *dest, u32 dlen, struct txn_table *t)
{
  struct txn_serialize_ctx ctx = {
    .s = srlizr_create (dest, dlen),
  };

  adptv_htable_foreach (&t->t, hnode_foreach_serialize, &ctx);

  return ctx.s.dlen;
}

err_t
txnt_deserialize (struct txn_table *dest, const u8 *src, struct txn *txn_bank, u32 slen, error *e)
{
  if (txnt_open (dest, e))
    {
      return e->cause_code;
    }

  if (slen == 0)
    {
      return SUCCESS;
    }

  struct deserializer d = dsrlizr_create (src, slen);

  ASSERT (slen % TXN_SERIAL_UNIT == 0);
  u32 tlen = slen / TXN_SERIAL_UNIT;

  for (u32 i = 0; i < tlen; ++i)
    {
      txid tid;
      lsn last_lsn;
      lsn undo_next_lsn;

      dsrlizr_read_expect (&tid, sizeof (tid), &d);
      dsrlizr_read_expect (&last_lsn, sizeof (last_lsn), &d);
      dsrlizr_read_expect (&undo_next_lsn, sizeof (undo_next_lsn), &d);

      txn_init (&txn_bank[i], tid, (struct txn_data){
                                       .last_lsn = last_lsn,
                                       .undo_next_lsn = undo_next_lsn,
                                       .state = TX_CANDIDATE_FOR_UNDO,
                                   });

      if (txnt_insert_txn (dest, &txn_bank[i], e))
        {
          goto failed;
        }
    }

  return SUCCESS;

failed:
  txnt_close (dest);
  return e->cause_code;
}

u32
txnlen_from_serialized (u32 slen)
{
  ASSERT (slen % TXN_SERIAL_UNIT == 0);
  return slen / TXN_SERIAL_UNIT;
}

#ifndef NTEST
TEST (TT_UNIT, txnt_serialize_deserialize_empty)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  u8 buffer[4096];
  u32 size = txnt_serialize (buffer, sizeof (buffer), &t);
  test_assert (size == 0);

  struct txn txn_bank[10];
  struct txn_table t2;
  test_err_t_wrap (txnt_deserialize (&t2, buffer, txn_bank, size, &e), &e);

  test_assert (txnt_equal (&t, &t2));

  txnt_close (&t);
  txnt_close (&t2);
}

TEST (TT_UNIT, txnt_serialize_deserialize_with_data)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  struct txn txns[10];
  for (int i = 0; i < 10; i++)
    {
      txn_init (&txns[i], i + 1, (struct txn_data){
                                     .last_lsn = (i + 1) * 100,
                                     .undo_next_lsn = (i + 1) * 100 - 10,
                                     .state = TX_RUNNING,
                                 });
      txnt_insert_txn (&t, &txns[i], &e);
    }

  u8 buffer[4096];
  u32 size = txnt_serialize (buffer, sizeof (buffer), &t);
  test_assert (size > 0);

  struct txn txn_bank[10];
  struct txn_table t2;
  test_err_t_wrap (txnt_deserialize (&t2, buffer, txn_bank, size, &e), &e);

  for (txid tid = 1; tid <= 10; tid++)
    {
      struct txn *retrieved;
      bool found = txnt_get (&retrieved, &t2, tid);
      test_assert (found);
      test_assert (retrieved->data.state == TX_CANDIDATE_FOR_UNDO);
    }

  txnt_close (&t);
  txnt_close (&t2);
}
#endif

#ifndef NTEST

struct txnt_eq_ctx
{
  struct txn_table *other;
  bool ret;
};

static void
txnt_eq_foreach (struct hnode *node, void *_ctx)
{
  struct txnt_eq_ctx *ctx = _ctx;
  if (ctx->ret == false)
    {
      return;
    }

  struct txn *tx = container_of (node, struct txn, node);
  struct txn candidate;

  latch_lock (&tx->l);
  {
    txn_key_init (&candidate, tx->tid);

    struct hnode *other_node = adptv_htable_lookup (&ctx->other->t, &candidate.node, txn_equals_for_exists);

    if (other_node == NULL)
      {
        ctx->ret = false;
        latch_unlock (&tx->l);
        return;
      }

    struct txn *other_tx = container_of (other_node, struct txn, node);

    latch_lock (&other_tx->l);
    {
      ctx->ret = txn_data_equal (&tx->data, &other_tx->data);
    }
    latch_unlock (&other_tx->l);
  }
  latch_unlock (&tx->l);
}

bool
txnt_equal (struct txn_table *left, struct txn_table *right)
{
  latch_lock (&left->l);
  latch_lock (&right->l);

  if (adptv_htable_size (&left->t) != adptv_htable_size (&right->t))
    {
      latch_unlock (&right->l);
      latch_unlock (&left->l);
      return false;
    }

  struct txnt_eq_ctx ctx = {
    .other = right,
    .ret = true,
  };
  adptv_htable_foreach (&left->t, txnt_eq_foreach, &ctx);

  latch_unlock (&right->l);
  latch_unlock (&left->l);

  return ctx.ret;
}

#ifndef NTEST
TEST (TT_UNIT, txnt_equal_empty_tables)
{
  error e = error_create ();
  struct txn_table t1, t2;
  test_err_t_wrap (txnt_open (&t1, &e), &e);
  test_err_t_wrap (txnt_open (&t2, &e), &e);

  test_assert (txnt_equal (&t1, &t2));

  txnt_close (&t1);
  txnt_close (&t2);
}

TEST (TT_UNIT, txnt_equal_same_content)
{
  error e = error_create ();
  struct txn_table t1, t2;
  test_err_t_wrap (txnt_open (&t1, &e), &e);
  test_err_t_wrap (txnt_open (&t2, &e), &e);

  struct txn t1_txns[5], t2_txns[5];
  for (int i = 0; i < 5; i++)
    {
      txn_init (&t1_txns[i], i + 1, (struct txn_data){
                                        .last_lsn = (i + 1) * 10,
                                        .undo_next_lsn = (i + 1) * 10 - 1,
                                        .state = TX_RUNNING,
                                    });
      txn_init (&t2_txns[i], i + 1, (struct txn_data){
                                        .last_lsn = (i + 1) * 10,
                                        .undo_next_lsn = (i + 1) * 10 - 1,
                                        .state = TX_RUNNING,
                                    });
      txnt_insert_txn (&t1, &t1_txns[i], &e);
      txnt_insert_txn (&t2, &t2_txns[i], &e);
    }

  test_assert (txnt_equal (&t1, &t2));

  txnt_close (&t1);
  txnt_close (&t2);
}

TEST (TT_UNIT, txnt_not_equal_different_content)
{
  error e = error_create ();
  struct txn_table t1, t2;
  test_err_t_wrap (txnt_open (&t1, &e), &e);
  test_err_t_wrap (txnt_open (&t2, &e), &e);

  struct txn tx1, tx2;
  txn_init (&tx1, 1, (struct txn_data){ .last_lsn = 10, .undo_next_lsn = 9, .state = TX_RUNNING });
  txn_init (&tx2, 1, (struct txn_data){ .last_lsn = 20, .undo_next_lsn = 19, .state = TX_RUNNING });

  txnt_insert_txn (&t1, &tx1, &e);
  txnt_insert_txn (&t2, &tx2, &e);

  test_assert (!txnt_equal (&t1, &t2));

  txnt_close (&t1);
  txnt_close (&t2);
}
#endif

err_t
txnt_rand_populate (struct txn_table *t, struct alloc *alloc, error *e)
{
  latch_lock (&t->l);
  u32 len = adptv_htable_size (&t->t);

  txid tid = 0;

  for (u32 i = 0; i < 1000 - len; ++i, tid += randu32r (0, 100))
    {
      struct txn *tx = alloc_alloc (alloc, 1, sizeof *t, NULL);
      if (t == NULL)
        {
          goto theend;
        }

      txn_init (tx, tid,
                (struct txn_data){
                    .last_lsn = randu32r (0, 1000),
                    .undo_next_lsn = randu32r (0, 1000),
                    .state = TX_RUNNING,
                });

      if (adptv_htable_insert (&t->t, &tx->node, e))
        {
          goto theend;
        }
    }

theend:
  latch_unlock (&t->l);
  return e->cause_code;
}

err_t
txnt_determ_populate (struct txn_table *t, struct alloc *alloc, error *e)
{
  latch_lock (&t->l);
  u32 len = adptv_htable_size (&t->t);

  txid tid = 0;

  for (u32 i = 0; i < 1000 - len; ++i, tid++)
    {
      struct txn *tx = alloc_alloc (alloc, 1, sizeof *t, NULL);
      if (t == NULL)
        {
          goto theend;
        }

      txn_init (tx, tid,
                (struct txn_data){
                    .last_lsn = i,
                    .undo_next_lsn = i,
                    .state = TX_RUNNING,
                });

      if (adptv_htable_insert (&t->t, &tx->node, e))
        {
          goto theend;
        }
    }

theend:
  latch_unlock (&t->l);
  return e->cause_code;
}

void
txnt_crash (struct txn_table *t)
{
  DBG_ASSERT (txn_table, t);
  adptv_htable_free (&t->t);
}

#endif

//////////////////////////////////////////////////
///// Concurrency Tests

#ifndef NTEST
struct txnt_thread_ctx
{
  struct txn_table *table;
  struct txn *txn_bank; // Pre-allocated transactions
  volatile int counter;
  txid start_tid;
  int count;
};

static void *
txnt_insert_thread (void *arg)
{
  struct txnt_thread_ctx *ctx = arg;
  error e = error_create ();

  for (int i = 0; i < ctx->count; i++)
    {
      txn_init (&ctx->txn_bank[i], ctx->start_tid + i,
                (struct txn_data){
                    .last_lsn = ctx->start_tid + i,
                    .undo_next_lsn = ctx->start_tid + i - 1,
                    .state = TX_RUNNING,
                });

      err_t_panic (txnt_insert_txn (ctx->table, &ctx->txn_bank[i], &e), &e);

      __sync_fetch_and_add (&ctx->counter, 1);
    }

  return NULL;
}

TEST (TT_UNIT, txnt_concurrent_inserts)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  struct txn txn_bank1[100], txn_bank2[100], txn_bank3[100];

  struct txnt_thread_ctx ctx1 = { .table = &t, .txn_bank = txn_bank1, .start_tid = 0, .count = 100, .counter = 0 };
  struct txnt_thread_ctx ctx2 = { .table = &t, .txn_bank = txn_bank2, .start_tid = 100, .count = 100, .counter = 0 };
  struct txnt_thread_ctx ctx3 = { .table = &t, .txn_bank = txn_bank3, .start_tid = 200, .count = 100, .counter = 0 };

  i_thread t1, t2, t3;
  test_assert_equal (i_thread_create (&t1, txnt_insert_thread, &ctx1, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, txnt_insert_thread, &ctx2, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t3, txnt_insert_thread, &ctx3, &e), SUCCESS);

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);
  test_err_t_wrap (i_thread_join (&t3, &e), &e);

  int total_inserts = ctx1.counter + ctx2.counter + ctx3.counter;
  test_assert_equal (total_inserts, 300);

  for (txid tid = 0; tid < 300; tid++)
    {
      test_assert (txn_exists (&t, tid));
    }

  txnt_close (&t);
}

static void *
txnt_reader_thread (void *arg)
{
  struct txnt_thread_ctx *ctx = arg;

  for (int i = 0; i < ctx->count; i++)
    {
      struct txn *retrieved;
      if (txnt_get (&retrieved, ctx->table, ctx->start_tid + i))
        {
          __sync_fetch_and_add (&ctx->counter, 1);
        }
    }

  return NULL;
}

TEST (TT_UNIT, txnt_concurrent_readers)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  // Pre-populate
  struct txn txns[200];
  for (int i = 0; i < 200; i++)
    {
      txn_init (&txns[i], i,
                (struct txn_data){
                    .last_lsn = i,
                    .undo_next_lsn = i - 1,
                    .state = TX_RUNNING,
                });
      txnt_insert_txn (&t, &txns[i], &e);
    }

  struct txnt_thread_ctx ctx1 = { .table = &t, .start_tid = 0, .count = 100, .counter = 0 };
  struct txnt_thread_ctx ctx2 = { .table = &t, .start_tid = 50, .count = 100, .counter = 0 };
  struct txnt_thread_ctx ctx3 = { .table = &t, .start_tid = 100, .count = 100, .counter = 0 };

  i_thread t1, t2, t3;
  test_assert_equal (i_thread_create (&t1, txnt_reader_thread, &ctx1, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, txnt_reader_thread, &ctx2, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t3, txnt_reader_thread, &ctx3, &e), SUCCESS);

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);
  test_err_t_wrap (i_thread_join (&t3, &e), &e);

  int total_reads = ctx1.counter + ctx2.counter + ctx3.counter;
  test_assert_equal (total_reads, 300);

  txnt_close (&t);
}

static void *
txnt_updater_thread (void *arg)
{
  struct txnt_thread_ctx *ctx = arg;

  for (int i = 0; i < ctx->count; i++)
    {
      struct txn *retrieved;
      if (txnt_get (&retrieved, ctx->table, ctx->start_tid + i))
        {
          struct txn_data new_data = retrieved->data;
          new_data.last_lsn = ctx->start_tid + i + 1000;
          txn_update_data (retrieved, new_data);
          __sync_fetch_and_add (&ctx->counter, 1);
        }
    }

  return NULL;
}

TEST (TT_UNIT, txnt_update_undo_next)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  struct txn tx;
  txn_init (&tx, 700, (struct txn_data){
                          .last_lsn = 100,
                          .undo_next_lsn = 80,
                          .state = TX_RUNNING,
                      });

  txnt_insert_txn (&t, &tx, &e);

  struct txn *retrieved;
  bool found = txnt_get (&retrieved, &t, 700);
  test_assert (found);

  struct txn_data new_data = retrieved->data;
  new_data.undo_next_lsn = 150;
  txn_update_data (retrieved, new_data);

  found = txnt_get (&retrieved, &t, 700);
  test_assert (found);
  test_assert (retrieved->data.undo_next_lsn == 150);

  txnt_close (&t);
}

TEST (TT_UNIT, txnt_update_state)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  struct txn tx;
  txn_init (&tx, 800, (struct txn_data){
                          .last_lsn = 100,
                          .undo_next_lsn = 90,
                          .state = TX_RUNNING,
                      });

  txnt_insert_txn (&t, &tx, &e);

  struct txn *retrieved;
  bool found = txnt_get (&retrieved, &t, 800);
  test_assert (found);

  struct txn_data new_data = retrieved->data;
  new_data.state = TX_COMMITTED;
  txn_update_data (retrieved, new_data);

  found = txnt_get (&retrieved, &t, 800);
  test_assert (found);
  test_assert (retrieved->data.state == TX_COMMITTED);

  txnt_close (&t);
}

TEST (TT_UNIT, txnt_concurrent_updates)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  // Pre-populate
  struct txn txns[300];
  for (int i = 0; i < 300; i++)
    {
      txn_init (&txns[i], i,
                (struct txn_data){
                    .last_lsn = i,
                    .undo_next_lsn = i - 1,
                    .state = TX_RUNNING,
                });
      txnt_insert_txn (&t, &txns[i], &e);
    }

  struct txnt_thread_ctx ctx1 = { .table = &t, .start_tid = 0, .count = 100, .counter = 0 };
  struct txnt_thread_ctx ctx2 = { .table = &t, .start_tid = 100, .count = 100, .counter = 0 };
  struct txnt_thread_ctx ctx3 = { .table = &t, .start_tid = 200, .count = 100, .counter = 0 };

  i_thread t1, t2, t3;
  test_assert_equal (i_thread_create (&t1, txnt_updater_thread, &ctx1, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, txnt_updater_thread, &ctx2, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t3, txnt_updater_thread, &ctx3, &e), SUCCESS);

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);
  test_err_t_wrap (i_thread_join (&t3, &e), &e);

  int total_updates = ctx1.counter + ctx2.counter + ctx3.counter;
  test_assert_equal (total_updates, 300);

  // Verify updates
  for (txid tid = 0; tid < 300; tid++)
    {
      struct txn *retrieved;
      test_assert (txnt_get (&retrieved, &t, tid));
      test_assert_equal (retrieved->data.last_lsn, tid + 1000);
    }

  txnt_close (&t);
}

static void *
txnt_state_transition_thread (void *arg)
{
  struct txnt_thread_ctx *ctx = arg;

  for (int i = 0; i < ctx->count; i++)
    {
      struct txn *retrieved;
      if (txnt_get (&retrieved, ctx->table, ctx->start_tid + i))
        {
          // TX_RUNNING -> TX_CANDIDATE_FOR_UNDO
          struct txn_data new_data = retrieved->data;
          new_data.state = TX_CANDIDATE_FOR_UNDO;
          txn_update_data (retrieved, new_data);

          __sync_fetch_and_add (&ctx->counter, 1);
          usleep (100);

          // -> TX_COMMITTED
          new_data.state = TX_COMMITTED;
          txn_update_data (retrieved, new_data);
        }
    }

  return NULL;
}

TEST (TT_UNIT, txnt_concurrent_state_transitions)
{
  error e = error_create ();
  struct txn_table t;
  test_err_t_wrap (txnt_open (&t, &e), &e);

  // Pre-populate with running transactions
  struct txn txns[150];
  for (int i = 0; i < 150; i++)
    {
      txn_init (&txns[i], i, (struct txn_data){
                                 .last_lsn = i,
                                 .undo_next_lsn = i - 1,
                                 .state = TX_RUNNING,
                             });
      txnt_insert_txn (&t, &txns[i], &e);
    }

  struct txnt_thread_ctx ctx1 = { .table = &t, .start_tid = 0, .count = 50, .counter = 0 };
  struct txnt_thread_ctx ctx2 = { .table = &t, .start_tid = 50, .count = 50, .counter = 0 };
  struct txnt_thread_ctx ctx3 = { .table = &t, .start_tid = 100, .count = 50, .counter = 0 };

  i_thread t1, t2, t3;
  test_assert_equal (i_thread_create (&t1, txnt_state_transition_thread, &ctx1, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t2, txnt_state_transition_thread, &ctx2, &e), SUCCESS);
  test_assert_equal (i_thread_create (&t3, txnt_state_transition_thread, &ctx3, &e), SUCCESS);

  test_err_t_wrap (i_thread_join (&t1, &e), &e);
  test_err_t_wrap (i_thread_join (&t2, &e), &e);
  test_err_t_wrap (i_thread_join (&t3, &e), &e);

  int total_transitions = ctx1.counter + ctx2.counter + ctx3.counter;
  test_assert_equal (total_transitions, 150);

  // Verify all are committed
  for (txid tid = 0; tid < 150; tid++)
    {
      struct txn *retrieved;
      test_assert (txnt_get (&retrieved, &t, tid));
      test_assert_equal (retrieved->data.state, TX_COMMITTED);
    }

  txnt_close (&t);
}

#endif
