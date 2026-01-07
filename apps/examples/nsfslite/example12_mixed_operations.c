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
 * Example 12: Mixed Operations
 *
 * Demonstrates a combination of insert, write, and remove operations on the same
 * dataset. Shows how different operations can be composed to perform complex data
 * transformations.
 *
 * Usage:
 *   ./example12_mixed_operations
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
  int *update = int_random (10000);
  int *read_data = int_random (N_ELEMS);

  unlink ("test12.db");
  unlink ("test12.wal");

  // OPEN
  CHECKN ((n = nsfslite_open ("test12.db", "test12.wal")));

  // NEW VARIABLE
  int64_t id = nsfslite_new (n, NULL, "data");
  CHECK (id);
  // INSERT
  CHECK (nsfslite_insert (n, id, NULL, data, 0, sizeof (int), N_ELEMS));

  for (size_t i = 0; i < 10000; i++)
    {
      update[i] = 55000 + i;
    }

  struct nsfslite_stride wstride = { .bstart = 50000 * sizeof (int), .stride = 1, .nelems = 10000 };
  // WRITE
  CHECK (nsfslite_write (n, id, NULL, update, sizeof (int), wstride));

  struct nsfslite_stride rstride_remove = { .bstart = 100000 * sizeof (int), .stride = 1, .nelems = 50000 };
  // REMOVE
  CHECK (nsfslite_remove (n, id, NULL, NULL, sizeof (int), rstride_remove));

  // CLOSE
  nsfslite_close (n);

  // OPEN
  CHECKN ((n = nsfslite_open ("test12.db", "test12.wal")));

  // GET ID
  id = nsfslite_get_id (n, "data");
  CHECK (id);

  size_t remaining = N_ELEMS - 50000;
  // READ
  struct nsfslite_stride rstride = { .bstart = 0, .stride = 1, .nelems = remaining };
  CHECK (nsfslite_read (n, id, read_data, sizeof (int), rstride));

  for (size_t i = 50000; i < 60000; i++)
    {
      int expected = 55000 + (i - 50000);
      if (read_data[i] != expected)
        {
          fprintf (stderr, "Mismatch in updated region at %zu: expected %d, got %d\n", i, expected, read_data[i]);
          ret = -1;
          goto cleanup;
        }
    }

  printf ("SUCCESS: Mixed insert/write/remove operations verified after reopen\n");

cleanup:
  if (n)
    {
      nsfslite_close (n);
    }
  free (data);
  free (read_data);
  free (update);
  return ret;
}
