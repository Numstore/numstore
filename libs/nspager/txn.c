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
 *   Implements txn.h. Transaction lifecycle management including begin, commit, abort, and rollback.
 */

#include "numstore/core/error.h"
#include "numstore/pager/lt_lock.h"
#include <numstore/pager/txn.h>

#include <numstore/core/hash_table.h>
#include <numstore/core/latch.h>

void txn_key_init (struct txn *dest, txid tid);

void
txn_init (struct txn *dest, txid tid, struct txn_data data)
{
  dest->data = data;
  dest->tid = tid;
  dest->locks = NULL;
  hnode_init (&dest->node, tid);
  latch_init (&dest->l);
}

void
txn_key_init (struct txn *dest, txid tid)
{
  dest->tid = tid;
  hnode_init (&dest->node, tid);
  latch_init (&dest->l);
}

void
txn_update (struct txn *t, struct txn_data data)
{
  latch_lock (&t->l);
  t->data = data;
  latch_unlock (&t->l);
}

bool
txn_data_equal (struct txn_data *left, struct txn_data *right)
{
  bool equal = true;

  equal = equal && left->last_lsn == right->last_lsn;
  equal = equal && left->undo_next_lsn == right->undo_next_lsn;
  equal = equal && left->state == right->state;

  return equal;
}

err_t
txn_newlock (struct txn *t, struct lt_lock lock, enum lock_mode mode, error *e)
{
  struct txn_lock *next = i_malloc (1, sizeof *next, e);
  if (next == NULL)
    {
      return e->cause_code;
    }

  next->lock = lock;
  next->mode = mode;

  latch_lock (&t->l);

  next->next = t->locks;
  t->locks = next;

  latch_unlock (&t->l);

  return SUCCESS;
}

bool
txn_haslock (struct txn *t, struct lt_lock lock)
{

  struct txn_lock *curr = t->locks;
  while (curr != NULL)
    {
      if (lt_lock_equal (curr->lock, lock))
        {
          latch_unlock (&t->l);
          return true;
        }
      curr = curr->next;
    }

  return false;
}

void
txn_free_all_locks (struct txn *t)
{
  struct txn_lock *curr = t->locks;
  while (curr != NULL)
    {
      struct txn_lock *next = curr->next;

      i_free (curr);

      curr = next;
    }
}

err_t
txn_foreach_lock (
    struct txn *t,
    err_t (*func) (struct lt_lock lock, enum lock_mode mode, void *ctx, error *e),
    void *ctx,
    error *e)
{
  struct txn_lock *curr = t->locks;
  while (curr != NULL)
    {
      err_t_wrap (func (curr->lock, curr->mode, ctx, e), e);
      curr = curr->next;
    }

  return SUCCESS;
}

void
i_log_txn (int log_level, struct txn *tx)
{
  i_log_info ("===================== TXN BEGIN ===================== \n");
  i_printf (log_level, "|%" PRtxid "| ", tx->tid);

  switch (tx->data.state)
    {
    case TX_RUNNING:
      {
        i_printf (log_level, "TX_RUNNING ");
        break;
      }
    case TX_CANDIDATE_FOR_UNDO:
      {
        i_printf (log_level, "TX_CANDIDATE_FOR_UNDO ");
        break;
      }
    case TX_COMMITTED:
      {
        i_printf (log_level, "TX_COMMITTED ");
        break;
      }
    case TX_DONE:
      {
        i_printf (log_level, "TX_DONE ");
        break;
      }
    }

  i_printf (log_level, "|last_lsn = %" PRtxid " undo_next_lsn = %" PRtxid "|\n", tx->data.last_lsn, tx->data.undo_next_lsn);

  struct txn_lock *curr = tx->locks;
  while (curr)
    {
      i_printf (log_level, "     |%3s| ", gr_lock_mode_name (curr->mode));
      i_print_lt_lock (log_level, curr->lock);
      curr = curr->next;
    }

  i_log_info ("===================== TXN END ===================== \n");
}
