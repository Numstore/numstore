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
 *   TODO: Add description for lock_table.h
 */

#include <numstore/core/adptv_hash_table.h>
#include <numstore/core/clock_allocator.h>
#include <numstore/core/gr_lock.h>
#include <numstore/core/hash_table.h>
#include <numstore/core/latch.h>
#include <numstore/pager/lt_lock.h>
#include <numstore/pager/txn.h>

#include <config.h>

struct lockt
{
  struct clck_alloc gr_lock_alloc; // Allocate gr locks
  struct adptv_htable table;       // The table of locks
};

err_t lockt_init (struct lockt *t, error *e);
void lockt_destroy (struct lockt *t);

/**
 * Locks are allocated by tx and appended to the tx list
 */
struct lt_lock *lockt_lock (
    struct lockt *t,
    enum lt_lock_type type,  // The type of lock you want to acquire
    union lt_lock_data data, // The data for this lock
    enum lock_mode mode,     // The lock mode you want
    struct txn *tx,          // Which transaction does this lock belong to
    error *e);

err_t lockt_upgrade (struct lockt *t, struct lt_lock *lock, enum lock_mode mode, error *e);

err_t lockt_unlock (struct lockt *t, struct txn *tx, error *e);

void i_log_lockt (int log_level, struct lockt *t);
