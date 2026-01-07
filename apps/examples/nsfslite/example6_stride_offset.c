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
 * Example 6: Combined Stride and Offset
 *
 * Demonstrates combined stride and offset operations. Shows how to perform strided
 * access starting from a specific byte offset, enabling complex access patterns in
 * multi-dimensional data structures.
 *
 * Usage:
 *   ./example6_stride_offset
 */

#include "nsfslite.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define N_ELEMS 200000
#define STRIDE 3
#define OFFSET 500

int
main (void)
{
  int ret = 0;
  nsfslite *n = NULL;
  size_t total_size = OFFSET + (N_ELEMS - 1) * STRIDE + 1;
  int *insert_data = calloc (total_size, sizeof (int));
  int *write_data = int_range (N_ELEMS);
  int *expected_data = calloc (total_size, sizeof (int));
  int *read_data = int_random (N_ELEMS);

  for (size_t i = 0; i < total_size; i++)
    {
      insert_data[i] = 1000 + i;
    }

  // Build expected result
  memcpy (expected_data, insert_data, total_size * sizeof (int));
  for (size_t i = 0; i < N_ELEMS; i++)
    {
      expected_data[OFFSET + i * STRIDE] = write_data[i];
    }

  unlink ("test6.db");
  unlink ("test6.wal");

  // OPEN
  CHECKN ((n = nsfslite_open ("test6.db", "test6.wal")));

  // NEW VARIABLE
  int64_t id = nsfslite_new (n, NULL, "data");
  CHECK (id);

  // INSERT
  CHECK (nsfslite_insert (n, id, NULL, insert_data, 0, sizeof (int), total_size));

  // WRITE
  struct nsfslite_stride wstride = { .bstart = OFFSET * sizeof (int), .stride = STRIDE, .nelems = N_ELEMS };
  CHECK (nsfslite_write (n, id, NULL, write_data, sizeof (int), wstride));

  // CLOSE
  nsfslite_close (n);

  // OPEN
  CHECKN ((n = nsfslite_open ("test6.db", "test6.wal")));

  // GET ID
  id = nsfslite_get_id (n, "data");
  CHECK (id);

  // READ
  struct nsfslite_stride rstride = { .bstart = OFFSET * sizeof (int), .stride = STRIDE, .nelems = N_ELEMS };
  CHECK (nsfslite_read (n, id, read_data, sizeof (int), rstride));

  // COMPARE
  COMPARE (write_data, read_data, N_ELEMS);

  printf ("SUCCESS: All %d elements match with stride=%d, offset=%d after reopen\n", N_ELEMS, STRIDE, OFFSET);

cleanup:
  if (n)
    {
      nsfslite_close (n);
    }
  free (insert_data);
  free (write_data);
  free (expected_data);
  free (read_data);
  return ret;
}
