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

#include <numstore/pager/txn.h>

#include <numstore/core/hash_table.h>
#include <numstore/core/spx_latch.h>

void txn_key_init (struct txn *dest, txid tid);

void
txn_init (struct txn *dest, txid tid, struct txn_data data)
{
  dest->data = data;
  dest->tid = tid;
  hnode_init (&dest->node, tid);
  spx_latch_init (&dest->l);
}

void
txn_update (struct txn *t, struct txn_data data)
{
  spx_latch_lock_x (&t->l);
  t->data = data;
  spx_latch_unlock_x (&t->l);
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
  spx_latch_init (&dest->l);
}
