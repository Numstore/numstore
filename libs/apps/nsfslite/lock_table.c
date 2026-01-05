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
 *   Implements lock_table.h. Provides a hierarchical lock table implementation for
 *   transaction concurrency control using two-phase locking (2PL). Manages lock acquisition
 *   and release for database resources including pages, variables, and internal structures.
 */

#include <lock_table.h>

#include <numstore/core/adptv_hash_table.h>
#include <numstore/core/assert.h>
#include <numstore/core/clock_allocator.h>
#include <numstore/core/error.h>
#include <numstore/core/gr_lock.h>
#include <numstore/core/hash_table.h>
#include <numstore/core/hashing.h>
#include <numstore/core/ht_models.h>
#include <numstore/core/spx_latch.h>

static void
lt_lock_init_key (struct lt_lock *dest, enum lt_lock_type type, union lt_lock_data data, struct txn *tx)
{
  ASSERT (dest);

  char hcode[sizeof (data) + sizeof (u8)];
  hcode[0] = type;
  u32 hcodelen = 1;

  switch (type)
    {
    case LOCK_DB:
    case LOCK_ROOT:
    case LOCK_FSTMBST:
    case LOCK_MSLSN:
    case LOCK_VHP:
      {
        break;
      }
    case LOCK_VHPOS:
      {
        hcodelen += i_memcpy (&hcode[hcodelen], &data.vhpos, sizeof (data.vhpos));
        break;
      }
    case LOCK_VAR:
      {
        hcodelen += i_memcpy (&hcode[hcodelen], &data.vhpos, sizeof (data.vhpos));
        break;
      }
    case LOCK_VAR_NEXT:
      {
        hcodelen += i_memcpy (&hcode[hcodelen], &data.var_root_next, sizeof (data.var_root_next));
        break;
      }
    case LOCK_RPTREE:
      {
        hcodelen += i_memcpy (&hcode[hcodelen], &data.rptree_root, sizeof (data.rptree_root));
        break;
      }
    }

  dest->type = type;
  dest->parent_tid = tx->tid;
  dest->data = data;

  struct cstring lock_type_hcode = {
    .data = hcode,
    .len = hcodelen,
  };

  hnode_init (&dest->lock_type_node, fnv1a_hash (lock_type_hcode));
  hnode_init (&dest->txn_node, (u32) (tx->tid % U32_MAX));
}

static void
lt_lock_init_txn_key (struct lt_lock *dest, struct txn *tx)
{
  dest->parent_tid = tx->tid;
  hnode_init (&dest->txn_node, (u32) (tx->tid % U32_MAX));
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
    }
  UNREACHABLE ();
}

static bool
lt_txn_lock_eq (const struct hnode *left, const struct hnode *right)
{
  const struct lt_lock *_left = container_of (left, struct lt_lock, txn_node);
  const struct lt_lock *_right = container_of (right, struct lt_lock, txn_node);

  return _left->parent_tid == _right->parent_tid;
}

err_t
nsfslt_init (struct nsfsllt *t, error *e)
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

  if (adptv_htable_init (&t->txn_index, settings, e))
    {
      clck_alloc_close (&t->gr_lock_alloc);
      adptv_htable_free (&t->table);
      return e->cause_code;
    }

  spx_latch_init (&t->l);

  return SUCCESS;
}

void
nsfslt_destroy (struct nsfsllt *t)
{
  // TODO - wait for all locks?
  clck_alloc_close (&t->gr_lock_alloc);
  adptv_htable_free (&t->table);
  adptv_htable_free (&t->txn_index);
}

err_t
nsfslock (
    struct nsfsllt *t,
    struct lt_lock *lock,
    enum lt_lock_type type,
    union lt_lock_data data,
    enum lock_mode mode,
    struct txn *tx,
    error *e)
{
  // Initialize the lock key
  lt_lock_init_key (lock, type, data, tx);

  // Lock the hash table
  spx_latch_lock_x (&t->l);

  // Find lock type that intersects this one
  struct hnode *node = adptv_htable_lookup (&t->table, &lock->lock_type_node, lt_lock_eq);

  struct gr_lock *_lock = NULL;

  // FOUND
  if (node != NULL)
    {
      // Lock already exists - try fast path
      struct lt_lock *existing = container_of (node, struct lt_lock, lock_type_node);
      _lock = existing->lock;

      if (!gr_trylock (_lock, mode))
        {
          panic ("TODO - lock coupling");
        }
    }
  else
    {
      // No existing lock - create a new one
      _lock = clck_alloc_alloc (&t->gr_lock_alloc, e);
      if (_lock == NULL)
        {
          spx_latch_unlock_x (&t->l);
          return e->cause_code;
        }

      if (gr_lock_init (_lock, e))
        {
          clck_alloc_free (&t->gr_lock_alloc, _lock);
          spx_latch_unlock_x (&t->l);
          return e->cause_code;
        }

      if (gr_lock (_lock, &(struct gr_lock_waiter){ .mode = mode }, e))
        {
          gr_lock_destroy (_lock);
          clck_alloc_free (&t->gr_lock_alloc, _lock);
          spx_latch_unlock_x (&t->l);
          return e->cause_code;
        }
    }

  // Common path: insert lock into tables
  lock->lock = _lock;

  // Insert into lock type table
  if (adptv_htable_insert (&t->table, &lock->lock_type_node, e))
    {
      gr_unlock (_lock, mode);
      if (node == NULL) // We created this lock
        {
          clck_alloc_free (&t->gr_lock_alloc, _lock);
        }
      spx_latch_unlock_x (&t->l);
      return e->cause_code;
    }

  // Insert into transaction index
  struct hnode *txn_node = adptv_htable_lookup (&t->txn_index, &lock->txn_node, lt_txn_lock_eq);
  if (txn_node != NULL)
    {
      // Transaction already has locks - add to linked list
      struct lt_lock *txn_head = container_of (txn_node, struct lt_lock, txn_node);
      lock->next = txn_head->next;
      txn_head->next = lock;
    }
  else
    {
      // First lock for this transaction
      lock->next = NULL;
      if (adptv_htable_insert (&t->txn_index, &lock->txn_node, e))
        {
          // Rollback: remove from lock type table
          adptv_htable_delete (NULL, &t->table, &lock->lock_type_node, lt_lock_eq, e);
          gr_unlock (_lock, mode);
          if (node == NULL) // We created this lock
            {
              clck_alloc_free (&t->gr_lock_alloc, _lock);
            }
          spx_latch_unlock_x (&t->l);
          return e->cause_code;
        }
    }

  spx_latch_unlock_x (&t->l);
  return SUCCESS;
}

err_t
nsfsunlock (struct nsfsllt *t, struct txn *tx, error *e)
{
  ASSERT (t);
  ASSERT (tx);

  spx_latch_lock_x (&t->l);

  // Lookup transaction's lock list
  struct lt_lock txn_key;
  lt_lock_init_txn_key (&txn_key, tx);

  struct hnode *txn_node = adptv_htable_lookup (&t->txn_index, &txn_key.txn_node, lt_txn_lock_eq);
  if (txn_node == NULL)
    {
      // No locks for this transaction
      spx_latch_unlock_x (&t->l);
      return SUCCESS;
    }

  // Remove from transaction index
  adptv_htable_delete (NULL, &t->txn_index, &txn_key.txn_node, lt_txn_lock_eq, e);

  // Walk the linked list and unlock everything
  struct lt_lock *curr = container_of (txn_node, struct lt_lock, txn_node);
  while (curr != NULL)
    {
      struct lt_lock *next = curr->next;

      // Remove from lock type table
      adptv_htable_delete (NULL, &t->table, &curr->lock_type_node, lt_lock_eq, e);

      // Unlock and check if we should free
      struct gr_lock *gr_lock_ptr = curr->lock;

      bool is_free = gr_unlock (gr_lock_ptr, curr->mode);

      if (is_free)
        {
          gr_lock_destroy (gr_lock_ptr);
          clck_alloc_free (&t->gr_lock_alloc, gr_lock_ptr);
        }

      curr = next;
    }

  spx_latch_unlock_x (&t->l);
  return SUCCESS;
}
