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
 * Example 15: Large-scale Remove
 *
 * Demonstrates removing a large number of elements using a stride pattern. Shows
 * how nsfslite efficiently handles bulk deletion operations on large datasets.
 *
 * Usage:
 *   ./example15_large_remove
 */

#include "nsfslite.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define N_ELEMS 200000
#define REMOVE_STRIDE 3
#define REMOVE_COUNT 50000

int
main (void)
{
  int ret = 0;
  nsfslite *n = NULL;
  int *data = int_range (N_ELEMS);
  int *read_data = int_range (N_ELEMS);

  unlink ("test15.db");
  unlink ("test15.wal");

  // OPEN
  CHECKN ((n = nsfslite_open ("test15.db", "test15.wal")));

  // NEW VARIABLE
  int64_t id = nsfslite_new (n, NULL, "data");
  CHECK (id);
  // INSERT
  CHECK (nsfslite_insert (n, id, NULL, data, 0, sizeof (int), N_ELEMS));

  struct nsfslite_stride rstride_remove = { .bstart = 0, .stride = REMOVE_STRIDE, .nelems = REMOVE_COUNT };
  // REMOVE
  CHECK (nsfslite_remove (n, id, NULL, NULL, sizeof (int), rstride_remove));

  // CLOSE
  nsfslite_close (n);

  // OPEN
  CHECKN ((n = nsfslite_open ("test15.db", "test15.wal")));

  // GET ID
  id = nsfslite_get_id (n, "data");
  CHECK (id);

  size_t remaining = N_ELEMS - REMOVE_COUNT;
  // READ
  struct nsfslite_stride rstride = { .bstart = 0, .stride = 1, .nelems = remaining };
  CHECK (nsfslite_read (n, id, read_data, sizeof (int), rstride));

  printf ("SUCCESS: Removed %d elements with stride=%d from %d, %zu remaining\n", REMOVE_COUNT, REMOVE_STRIDE, N_ELEMS, remaining);

cleanup:
  if (n)
    {
      nsfslite_close (n);
    }
  free (data);
  free (read_data);
  return ret;
}
