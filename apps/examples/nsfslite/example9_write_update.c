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
 * Example 9: Update Existing Data
 *
 * Demonstrates updating existing data using write operations. Shows how to
 * overwrite a portion of existing data without changing the total size, useful
 * for in-place modifications.
 *
 * Usage:
 *   ./example9_write_update
 */

#include "nsfslite.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define N_ELEMS 200000
#define UPDATE_OFFSET 10000
#define UPDATE_COUNT 50000

int
main (void)
{
  int ret = 0;
  nsfslite *n = NULL;
  int *data = malloc (N_ELEMS * sizeof (int));
  int *update_data = malloc (UPDATE_COUNT * sizeof (int));
  int *read_data = int_random (N_ELEMS);
  int *expected = malloc (N_ELEMS * sizeof (int));

  for (size_t i = 0; i < N_ELEMS; i++)
    {
      data[i] = i;
      expected[i] = i;
    }
  for (size_t i = 0; i < UPDATE_COUNT; i++)
    {
      update_data[i] = 88000 + i;
    }
  for (size_t i = 0; i < UPDATE_COUNT; i++)
    {
      expected[UPDATE_OFFSET + i] = 88000 + i;
    }

  unlink ("test9.db");
  unlink ("test9.wal");

  // OPEN
  CHECKN ((n = nsfslite_open ("test9.db", "test9.wal")));

  // NEW VARIABLE
  int64_t id = nsfslite_new (n, NULL, "data");
  CHECK (id);
  // INSERT
  CHECK (nsfslite_insert (n, id, NULL, data, 0, sizeof (int), N_ELEMS));

  struct nsfslite_stride wstride = { .bstart = UPDATE_OFFSET * sizeof (int), .stride = 1, .nelems = UPDATE_COUNT };
  // WRITE
  CHECK (nsfslite_write (n, id, NULL, update_data, sizeof (int), wstride));

  // CLOSE
  nsfslite_close (n);

  // OPEN
  CHECKN ((n = nsfslite_open ("test9.db", "test9.wal")));

  // GET ID
  id = nsfslite_get_id (n, "data");
  CHECK (id);

  // READ
  struct nsfslite_stride rstride = { .bstart = 0, .stride = 1, .nelems = N_ELEMS };
  CHECK (nsfslite_read (n, id, read_data, sizeof (int), rstride));

  // COMPARE
  COMPARE (expected, read_data, N_ELEMS);

  printf ("SUCCESS: Updated %d elements at offset %d, verified after reopen\n", UPDATE_COUNT, UPDATE_OFFSET);

cleanup:
  if (n)
    {
      nsfslite_close (n);
    }
  free (data);
  free (update_data);
  free (read_data);
  free (expected);
  return ret;
}
