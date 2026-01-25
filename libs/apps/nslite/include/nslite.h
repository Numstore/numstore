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
 *   Public C API header for NumStore Lite (nslite). Provides a simplified
 *   interface for managing named variables in a transactional database with support for
 *   create, read, write, insert, remove operations using stride-based access patterns.
 *   Supports both implicit and explicit transaction management.
 */

#pragma once

#include <numstore/core/error.h>
#include <numstore/intf/types.h>

typedef struct nslite_s nslite;
typedef struct txn nslite_txn;

struct nslite_stride
{
  b_size bstart;
  u32 stride;
  b_size nelems;
};

nslite *nslite_open (const char *fname, const char *recovery, error *e);
err_t nslite_close (nslite *n, error *e);

nslite_txn *nslite_begin_txn (nslite *n, error *e);
err_t nslite_commit (nslite *n, nslite_txn *tx, error *e);
err_t nslite_rollback (nslite *n, nslite_txn *tx, error *e);
spgno nslite_new (nslite *n, nslite_txn *tx, error *e);
err_t nslite_delete (nslite *n, nslite_txn *tx, pgno start, error *e);
sb_size nslite_size (nslite *n, pgno id, error *e);

err_t nslite_insert (
    nslite *n,
    pgno id,
    nslite_txn *tx,
    const void *src,
    b_size bot,
    t_size size,
    b_size nelem,
    error *e);

err_t nslite_write (
    nslite *n,
    pgno id,
    nslite_txn *tx,
    const void *src,
    t_size size,
    struct nslite_stride stride,
    error *e);

sb_size nslite_read (
    nslite *n,
    pgno id,
    void *dest,
    t_size size,
    struct nslite_stride stride,
    error *e);

err_t nslite_remove (
    nslite *n,
    pgno id,
    nslite_txn *tx,
    void *dest,
    t_size size,
    struct nslite_stride stride,
    error *e);
