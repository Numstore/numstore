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
 * Example 5: Offset-based Operations
 *
 * Demonstrates offset-based write and read operations. Shows how to write and read
 * data starting at a specific byte offset in the storage, enabling random access
 * patterns.
 *
 * Usage:
 *   ./example5_offset
 */

#include "nsfslite.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define N_ELEMS 200000
#define OFFSET 1000

int
main (void)
{
  int ret = 0;
  nsfslite *n = NULL;
  int *data = int_range (N_ELEMS);
  int *read_data = int_random (N_ELEMS);

  unlink ("test5.db");
  unlink ("test5.wal");

  // OPEN
  CHECKN ((n = nsfslite_open ("test5.db", "test5.wal")));

  // NEW VARIABLE
  int64_t id = nsfslite_new (n, NULL, "data");
  CHECK (id);

  // INSERT
  CHECK (nsfslite_insert (n, id, NULL, data, 0, sizeof (int), N_ELEMS));

  // CLOSE
  nsfslite_close (n);

  // OPEN
  CHECKN ((n = nsfslite_open ("test5.db", "test5.wal")));

  // GET ID
  id = nsfslite_get_id (n, "data");
  CHECK (id);

  // READ
  struct nsfslite_stride rstride = { .bstart = OFFSET * sizeof (int), .stride = 1, .nelems = N_ELEMS };
  CHECK (nsfslite_read (n, id, read_data, sizeof (int), rstride));

  // COMPARE
  COMPARE (&data[OFFSET], read_data, N_ELEMS - OFFSET);

  printf ("SUCCESS: All %d elements match with offset=%d after reopen\n", N_ELEMS, OFFSET);

cleanup:
  if (n)
    {
      nsfslite_close (n);
    }
  free (data);
  free (read_data);
  return ret;
}
