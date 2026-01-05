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

#include "numstore/intf/os.h"
#include <numstore/pager/txn.h>

#include <numstore/core/hash_table.h>
#include <numstore/core/latch.h>

void txn_key_init (struct txn *dest, txid tid);

void
txn_init (struct txn *dest, txid tid, struct txn_data data)
{
  dest->data = data;
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

void
txn_key_init (struct txn *dest, txid tid)
{
  dest->tid = tid;
  hnode_init (&dest->node, tid);
  latch_init (&dest->l);
}

struct lt_lock *
txn_newlock (struct txn *t, enum lt_lock_type type, union lt_lock_data data, enum lock_mode mode, error *e)
{
  struct lt_lock *lock = i_malloc (1, sizeof *lock, e);
  if (lock == NULL)
    {
      return NULL;
    }
  lock->type = type;
  lock->mode = mode;
  lock->data = data;

  latch_lock (&t->l);

  lock->next = t->locks;
  t->locks = lock;

  latch_unlock (&t->l);

  return lock;
}

void
txn_freelock (struct txn *t, struct lt_lock *lock)
{
  if (lock->prev != NULL)
    {
      lock->prev->next = lock->next;
    }
  if (lock->next != NULL)
    {
      lock->next->prev = lock->prev;
    }

  if (t->locks == lock)
    {
      t->locks = lock->next;
    }

  i_free (lock);
}

void
txn_free_all_locks (struct txn *t)
{
  latch_lock (&t->l);
  struct lt_lock *cur = t->locks;
  t->locks = NULL;
  latch_lock (&t->l);

  while (cur)
    {
      latch_lock (&cur->l); // TODO - do I need to do this?

      ASSERT (cur->lock == NULL);
      struct lt_lock *next = cur->next;
      gr_unlock (cur->lock, cur->mode);
      i_free (cur);

      cur = next;
    }
}
