/*
 * Copyright (c) 2025 Theo Lincke
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
 * Example 14: Transaction Support
 *
 * Demonstrates transaction support with begin/commit operations. Shows how to
 * perform atomic operations that are only persisted when explicitly committed,
 * ensuring data consistency.
 *
 * Usage:
 *   ./example14_txn
 */

#include "nsfslite.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define N_ELEMS 200000

int
main (void)
{
  int ret = 0;
  nsfslite *n = NULL;
  int *data = int_range (N_ELEMS);
  int *read_data = int_random (N_ELEMS);

  unlink ("test14.db");
  unlink ("test14.wal");

  // OPEN
  CHECKN ((n = nsfslite_open ("test14.db", "test14.wal")));

  // NEW VARIABLE
  int64_t id = nsfslite_new (n, NULL, "data");
  CHECK (id);

  nsfslite_txn *tx = nsfslite_begin_txn (n);
  if (!tx)
    {
      fprintf (stderr, "Failed to begin transaction: %s\n", nsfslite_error (n));
      ret = -1;
      goto cleanup;
    }

  // INSERT
  CHECK (nsfslite_insert (n, id, tx, data, 0, sizeof (int), N_ELEMS));
  CHECK (nsfslite_commit (n, tx));

  // CLOSE
  nsfslite_close (n);

  // OPEN
  CHECKN ((n = nsfslite_open ("test14.db", "test14.wal")));

  // GET ID
  id = nsfslite_get_id (n, "data");
  CHECK (id);

  // READ
  struct nsfslite_stride rstride = { .bstart = 0, .stride = 1, .nelems = N_ELEMS };
  CHECK (nsfslite_read (n, id, read_data, sizeof (int), rstride));

  // COMPARE
  COMPARE (data, read_data, N_ELEMS);

  printf ("SUCCESS: Transaction with %d elements committed and verified\n", N_ELEMS);

cleanup:
  if (n)
    {
      nsfslite_close (n);
    }
  free (data);
  free (read_data);
  return ret;
}
