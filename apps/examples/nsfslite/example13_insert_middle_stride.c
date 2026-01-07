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
 * Example 13: Insert with Strided Read
 *
 * Demonstrates inserting data in the middle of a dataset, then reading the result
 * using a stride pattern. Shows how insertion affects subsequent strided access
 * patterns.
 *
 * Usage:
 *   ./example13_insert_middle_stride
 */

#include "nsfslite.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define N_ELEMS 20000
#define INSERT_COUNT 5000
#define INSERT_OFFSET 10000
#define STRIDE 2

int
main (void)
{
  int ret = 0;
  nsfslite *n = NULL;
  int *data = int_range (N_ELEMS);
  int *insert_data = int_random (INSERT_COUNT);
  int *read_data = int_random (N_ELEMS + INSERT_COUNT);
  for (size_t i = 0; i < INSERT_COUNT; i++)
    {
      insert_data[i] = 66000 + i;
    }

  unlink ("test13.db");
  unlink ("test13.wal");

  // OPEN
  CHECKN ((n = nsfslite_open ("test13.db", "test13.wal")));

  // NEW VARIABLE
  int64_t id = nsfslite_new (n, NULL, "data");
  CHECK (id);

  // INSERT
  CHECK (nsfslite_insert (n, id, NULL, data, 0, sizeof (int), N_ELEMS));
  CHECK (nsfslite_insert (n, id, NULL, insert_data, INSERT_OFFSET * sizeof (int), sizeof (int), INSERT_COUNT));

  // CLOSE
  nsfslite_close (n);

  // OPEN
  CHECKN ((n = nsfslite_open ("test13.db", "test13.wal")));

  // GET ID
  id = nsfslite_get_id (n, "data");
  CHECK (id);

  // READ
  size_t read_count = (N_ELEMS + INSERT_COUNT) / STRIDE;
  struct nsfslite_stride rstride = { .bstart = 0, .stride = STRIDE, .nelems = read_count };
  CHECK (nsfslite_read (n, id, read_data, sizeof (int), rstride));

  printf ("SUCCESS: Inserted %d elements in middle, read %zu with stride=%d after reopen\n", INSERT_COUNT, read_count, STRIDE);

cleanup:
  if (n)
    {
      nsfslite_close (n);
    }
  free (data);
  free (insert_data);
  free (read_data);
  return ret;
}
