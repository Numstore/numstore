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
 *   Public C API header for NumStore File (nsfile). 
 */

#pragma once

#include <numstore/core/error.h>
#include <numstore/core/stride.h>
#include <numstore/intf/types.h>

typedef struct nsfile_s nsfile;

nsfile *nsfile_open (const char *fname, const char *recovery, error *e);
err_t nsfile_close (nsfile *n, error *e);

struct txn *nsfile_begin_txn (nsfile *n, error *e);
err_t nsfile_commit (nsfile *n, struct txn *tx, error *e);
err_t nsfile_rollback (nsfile *n, struct txn *tx, error *e);
sb_size nsfile_size (nsfile *n, error *e);

err_t nsfile_insert (
    nsfile *n,
    struct txn *tx,
    const void *src,
    b_size bot,
    t_size size,
    b_size nelem,
    error *e);

err_t nsfile_write (
    nsfile *n,
    struct txn *tx,
    const void *src,
    t_size size,
    struct stride stride,
    error *e);

sb_size nsfile_read (
    nsfile *n,
    void *dest,
    t_size size,
    struct stride stride,
    error *e);

err_t nsfile_remove (
    nsfile *n,
    struct txn *tx,
    void *dest,
    t_size size,
    struct stride stride,
    error *e);
