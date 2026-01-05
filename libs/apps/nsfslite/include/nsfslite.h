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

#ifndef NSFSLITE
#define NSFSLITE

#include <stdint.h>
#include <stdlib.h>

typedef struct nsfslite_s nsfslite; // An opaque type to hold backend data structures
typedef struct txn nsfslite_txn;    // An opaque transaction type

// Error handling
const char *nsfslite_error (nsfslite *n);
void nsfslite_reset_errors (nsfslite *n);

// Lifecycle
nsfslite *nsfslite_open (const char *fname, const char *recovery_fname);
int nsfslite_close (nsfslite *n);

// Stride parameters
struct nsfslite_stride
{
  size_t bstart;
  size_t stride;
  size_t nelems;
};

// BEGIN TXN
nsfslite_txn *nsfslite_begin_txn (nsfslite *n);

// COMMIT
int nsfslite_commit (nsfslite *n, nsfslite_txn *tx);

// Create
int64_t nsfslite_new (
    nsfslite *n,      // nsfslite handle
    nsfslite_txn *tx, // Transaction or NULL for implicit transaction
    const char *name  // The variable name for this variable
);

// Get
int64_t nsfslite_get_id (
    nsfslite *n,     // nsfslite handle
    const char *name // The variable name to get
);

// Delete
int nsfslite_delete (
    nsfslite *n,      // nsfslite handle
    nsfslite_txn *tx, // Transaction or NULL for implicit transaction
    const char *name  // The variable name for this variable
);

// Variable size
size_t nsfslite_fsize (
    nsfslite *n, // nsfslite handle
    uint64_t id  // The variable id from nsfslite_get_id
);

// Insert
ssize_t nsfslite_insert (
    nsfslite *n,      // nsfslite handle
    uint64_t id,      // variable id - from nsfslite_get_id
    nsfslite_txn *tx, // transaction or NULL for implicit transaction
    const void *src,  // Source data
    size_t bofst,     // Starting (byte) offset to insert into
    size_t size,      // Size of each element
    size_t nelem      // number of elements
);

// Write
ssize_t nsfslite_write (
    nsfslite *n,                  // nsfslite handle
    uint64_t id,                  // variable id - from nsfslite_get_id
    nsfslite_txn *tx,             // transaction or NULL for implicit transaction
    const void *src,              // Source data
    size_t size,                  // Size of each element
    struct nsfslite_stride stride // Stride pattern
);

// Read
ssize_t nsfslite_read (
    nsfslite *n,                  // nsfslite handle
    uint64_t id,                  // variable id - from nsfslite_get_id
    void *dest,                   // Destination buffer
    size_t size,                  // Size of each element
    struct nsfslite_stride stride // Stride pattern
);

// Remove
ssize_t nsfslite_remove (
    nsfslite *n,                  // nsfslite handle
    uint64_t id,                  // variable id - from nsfslite_get_id
    nsfslite_txn *tx,             // transaction or NULL for implicit transaction
    void *dest,                   // Optional destination buffer
    size_t size,                  // Size of each element
    struct nsfslite_stride stride // stride pattern
);

#endif
