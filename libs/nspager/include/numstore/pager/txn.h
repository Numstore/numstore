#pragma once

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
 *   TODO: Add description for txn.h
 */

#include <numstore/core/hash_table.h>
#include <numstore/core/latch.h>
#include <numstore/intf/types.h>
#include <numstore/pager/lt_lock.h>

struct txn_data
{
  /**
   * In the ARIES paper:
   *   - P (prepared / in doubt) - Transaction has 
   *     completed it's prepare phase in two phase
   *     commit but hasn't yet received a commit / abort decision
   *   - U (unprepared) - Transaction is active and hasn't prepared yet
   */
  enum tx_state
  {
    /**
     * Not in the original ARIES paper,
     * using this for a placeholder during
     * normal execution, generally, it's just
     * removed once it's committed
     */
    TX_RUNNING,

    /**
     * During restart, this just says we haven't
     * received a commit record yet, so we'll need
     * to remove it if it stays like this
     */
    TX_CANDIDATE_FOR_UNDO, // (U)

    /**
     * Never actually set in normal processing - use
     * this in the restart recovery algorithm to know which
     * tx's to append end to
     */
    TX_COMMITTED, // (P)

    /**
     * Just a special case for done transactions with end record 
     * appended. You'd just remove the transaction from the 
     * table when done
     */
    TX_DONE,
  } state;

  lsn last_lsn;
  lsn undo_next_lsn;
};

struct txn
{
  txid tid;              // Transaction id
  struct txn_data data;  // The transaction data
  struct hnode node;     // The node that indicates where this txn is in the att
  struct lt_lock *locks; // All held locks for this transaction
  struct latch l;        // Thread safety
};

void txn_init (struct txn *dest, txid tid, struct txn_data data);
void txn_update (struct txn *t, struct txn_data data);
bool txn_data_equal (struct txn_data *left, struct txn_data *right);
void txn_key_init (struct txn *dest, txid tid);
struct lt_lock *txn_newlock (struct txn *t, enum lt_lock_type type, union lt_lock_data data, enum lock_mode mode, error *e);
void txn_freelock (struct txn *t, struct lt_lock *lock);
void txn_free_all_locks (struct txn *t);
