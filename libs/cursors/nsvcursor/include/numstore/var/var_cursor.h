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
 *   Defines the variable cursor structure and operations for managing named
 *   variables in the database, providing create, get, and delete functionality
 *   with type tracking.
 */

#include <numstore/core/assert.h>
#include <numstore/core/chunk_alloc.h>
#include <numstore/core/latch.h>
#include <numstore/pager.h>
#include <numstore/pager/page_h.h>
#include <numstore/var/attr.h>

#include "config.h"

struct var_cursor
{
  struct latch latch;
  struct pager *pager;
  page_h cur;
  u32 tidx;
  struct txn *tx;

  // INPUTS
  u8 vstr_input[MAX_VSTR];
  u32 vlen_input;

  u8 tstr_input[MAX_TSTR];
  u32 tlen_input;

  u8 vstr[MAX_VSTR];
  u32 vlen;

  u8 tstr[MAX_TSTR];
  u32 tlen;
};

DEFINE_DBG_ASSERT (
    struct var_cursor, var_cursor, v, {
      ASSERT (v);
      ASSERT (v->pager);
    })

// Common
err_t varh_init_hash_page (struct pager *p, error *e);
err_t varc_initialize (struct var_cursor *v, struct pager *p, error *e);

// Logging
void i_log_var_cursor (int log_level, struct var_cursor *r);

// Runtime
spgno vpc_new (
    struct var_cursor *v,
    struct var_create_params params,
    error *e);

spgno vpc_get (
    struct var_cursor *v,
    struct chunk_alloc *dalloc,
    struct var_get_params *dest,
    error *e);

err_t vpc_update_by_id (
    struct var_cursor *v,
    struct var_update_by_id_params *src,
    error *e);

err_t vpc_get_by_id (
    struct var_cursor *v,
    struct chunk_alloc *dalloc,
    struct var_get_by_id_params *dest,
    error *e);

err_t vpc_delete (
    struct var_cursor *v,
    const struct string name,
    error *e);

// Transactions
void varc_enter_transaction (struct var_cursor *r, struct txn *tx);
void varc_leave_transaction (struct var_cursor *r);
#define varc_maybe_leave_transaction(r) \
  do                                    \
    {                                   \
      if ((r)->tx >= 0)                 \
        {                               \
          varc_leave_transaction (r);   \
        }                               \
    }                                   \
  while (0)
