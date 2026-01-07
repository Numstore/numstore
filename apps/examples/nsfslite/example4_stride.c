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
 * Example 4: Strided Operations
 *
 * Demonstrates strided write and read operations. Shows how to write and read data
 * at regular intervals (stride) rather than contiguously, useful for accessing
 * specific patterns in multi-dimensional arrays.
 *
 * Usage:
 *   ./example4_stride
 */

#include "nsfslite.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define N_ELEMS 1000000
#define STRIDE 5

int
main (void)
{
  int ret = 0;
  nsfslite *n = NULL;
  int *insert_data = calloc (N_ELEMS, sizeof (int));
  int *write_data = calloc (N_ELEMS, sizeof (int));
  int *expected_data = calloc (N_ELEMS, sizeof (int));
  int *read_data = calloc (N_ELEMS, sizeof (int));

  for (size_t i = 0; i < N_ELEMS; i++)
    {
      insert_data[i] = 1000 + i;
    }

  for (size_t i = 0; i < N_ELEMS; i++)
    {
      write_data[i] = 2000 + i;
    }

  // Build expected result
  memcpy (expected_data, insert_data, N_ELEMS * sizeof (int));
  for (size_t i = 0; i < N_ELEMS / STRIDE; i++)
    {
      expected_data[i * STRIDE] = write_data[i];
    }

  unlink ("test4.db");
  unlink ("test4.wal");

  // OPEN
  CHECKN ((n = nsfslite_open ("test4.db", "test4.wal")));

  // NEW VARIABLE
  int64_t id = nsfslite_new (n, NULL, "data");
  CHECK (id);

  // INSERT
  CHECK (nsfslite_insert (n, id, NULL, insert_data, 0, sizeof (int), N_ELEMS));

  // WRITE
  struct nsfslite_stride wstride = { .bstart = 0, .stride = STRIDE, .nelems = N_ELEMS / STRIDE };
  CHECK (nsfslite_write (n, id, NULL, write_data, sizeof (int), wstride));

  // CLOSE
  nsfslite_close (n);

  // OPEN
  CHECKN ((n = nsfslite_open ("test4.db", "test4.wal")));

  // GET ID
  id = nsfslite_get_id (n, "data");
  CHECK (id);

  // READ
  struct nsfslite_stride rstride = { .bstart = 0, .stride = 1, .nelems = N_ELEMS };
  CHECK (nsfslite_read (n, id, read_data, sizeof (int), rstride));

  // COMPARE
  COMPARE (expected_data, read_data, N_ELEMS);

  printf ("SUCCESS: All %d elements match with stride=%d after reopen\n", N_ELEMS, STRIDE);

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
