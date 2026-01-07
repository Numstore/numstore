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
 * Example 11: Strided Write Operations
 *
 * Demonstrates strided write operations to update existing data at regular
 * intervals. Shows how to selectively modify elements in a dataset using stride
 * patterns.
 *
 * Usage:
 *   ./example11_write_stride
 */

#include "nsfslite.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define N_ELEMS 20000
#define WRITE_STRIDE 7
#define WRITE_COUNT 2000

int
main (void)
{
  int ret = 0;
  nsfslite *n = NULL;
  int *data = int_range (N_ELEMS);
  int *write_data = int_range (WRITE_COUNT);
  int *read_data = int_range (N_ELEMS);

  for (size_t i = 0; i < WRITE_COUNT; i++)
    {
      write_data[i] = 77000 + i;
    }

  unlink ("test11.db");
  unlink ("test11.wal");

  // OPEN
  CHECKN ((n = nsfslite_open ("test11.db", "test11.wal")));

  // NEW VARIABLE
  int64_t id = nsfslite_new (n, NULL, "data");
  CHECK (id);
  // INSERT
  CHECK (nsfslite_insert (n, id, NULL, data, 0, sizeof (int), N_ELEMS));

  struct nsfslite_stride wstride = { .bstart = 0, .stride = WRITE_STRIDE, .nelems = WRITE_COUNT };
  // WRITE
  CHECK (nsfslite_write (n, id, NULL, write_data, sizeof (int), wstride));

  // CLOSE
  nsfslite_close (n);

  // OPEN
  CHECKN ((n = nsfslite_open ("test11.db", "test11.wal")));

  // GET ID
  id = nsfslite_get_id (n, "data");
  CHECK (id);

  // READ
  struct nsfslite_stride rstride = { .bstart = 0, .stride = 1, .nelems = N_ELEMS };
  CHECK (nsfslite_read (n, id, read_data, sizeof (int), rstride));

  for (size_t i = 0; i < WRITE_COUNT; i++)
    {
      size_t idx = i * WRITE_STRIDE;
      if (read_data[idx] != write_data[i])
        {
          fprintf (stderr, "Mismatch at stride position %zu (index %zu): expected %d, got %d\n",
                   i, idx, write_data[i], read_data[idx]);
          ret = -1;
          goto cleanup;
        }
    }

  printf ("SUCCESS: Wrote %d elements with stride=%d, verified after reopen\n", WRITE_COUNT, WRITE_STRIDE);

cleanup:
  if (n)
    {
      nsfslite_close (n);
    }
  free (data);
  free (write_data);
  free (read_data);
  return ret;
}
