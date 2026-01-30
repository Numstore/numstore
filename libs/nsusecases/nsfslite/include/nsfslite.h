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
 *   Public C API header for NumStore File System Lite (nsfslite). Provides a simplified
 *   interface for managing named variables in a transactional database with support for
 *   create, read, write, insert, remove operations using stride-based access patterns.
 *   Supports both implicit and explicit transaction management.
 */

#pragma once

#include <numstore/core/error.h>
#include <numstore/core/stride.h>
#include <numstore/intf/types.h>

typedef struct nsfslite_s nsfslite;

nsfslite *nsfslite_open (const char *fname, const char *recovery_fname, error *e);
err_t nsfslite_close (nsfslite *n, error *e);

struct txn *nsfslite_begin_txn (nsfslite *n, error *e);
err_t nsfslite_commit (nsfslite *n, struct txn *tx, error *e);

spgno nsfslite_new (
    nsfslite *n,
    struct txn *tx,
    const char *name,
    error *e);

spgno nsfslite_get_id (
    nsfslite *n,
    const char *name,
    error *e);

err_t nsfslite_delete (
    nsfslite *n,
    struct txn *tx,
    const char *name,
    error *e);

sb_size nsfslite_fsize (
    nsfslite *n,
    pgno id,
    error *e);

err_t nsfslite_insert (
    nsfslite *n,
    pgno id,
    struct txn *tx,
    const void *src,
    b_size bofst,
    t_size size,
    b_size nelem,
    error *e);

err_t nsfslite_write (
    nsfslite *n,
    pgno id,
    struct txn *tx,
    const void *src,
    t_size size,
    struct stride stride,
    error *e);

sb_size nsfslite_read (
    nsfslite *n,
    pgno id,
    void *dest,
    t_size size,
    struct stride stride,
    error *e);

err_t nsfslite_remove (
    nsfslite *n,
    pgno id,
    struct txn *tx,
    void *dest,
    t_size size,
    struct stride stride,
    error *e);
