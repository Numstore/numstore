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
 * Example 10: Multiple Inserts
 *
 * Demonstrates performing many sequential insert operations. Shows how nsfslite
 * handles multiple appending inserts efficiently, building up a large dataset
 * incrementally.
 *
 * Usage:
 *   ./example10_many_inserts
 */

#include "nsfslite.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_INSERTS 1000
#define INSERT_SIZE 200

int
main (void)
{
  int ret = 0;
  nsfslite *n = NULL;
  int *chunk = malloc (INSERT_SIZE * sizeof (int));

  unlink ("test10.db");
  unlink ("test10.wal");

  // OPEN
  CHECKN ((n = nsfslite_open ("test10.db", "test10.wal")));

  // NEW VARIABLE
  int64_t id = nsfslite_new (n, NULL, "data");
  CHECK (id);

  size_t total_inserted = 0;
  for (size_t i = 0; i < NUM_INSERTS; i++)
    {
      for (size_t j = 0; j < INSERT_SIZE; j++)
        {
          chunk[j] = total_inserted + j;
        }

      // INSERT
      CHECK (nsfslite_insert (n, id, NULL, chunk, total_inserted * sizeof (int), sizeof (int), INSERT_SIZE));

      total_inserted += INSERT_SIZE;
    }

  // CLOSE
  nsfslite_close (n);

  // OPEN
  CHECKN ((n = nsfslite_open ("test10.db", "test10.wal")));

  // GET ID
  id = nsfslite_get_id (n, "data");
  CHECK (id);

  for (size_t i = 0; i < NUM_INSERTS; i++)
    {
      // READ
      struct nsfslite_stride rstride = { .bstart = i * INSERT_SIZE * sizeof (int), .stride = 1, .nelems = INSERT_SIZE };
      CHECK (nsfslite_read (n, id, chunk, sizeof (int), rstride));

      for (size_t j = 0; j < INSERT_SIZE; j++)
        {
          int expected = i * INSERT_SIZE + j;
          if (chunk[j] != expected)
            {
              fprintf (stderr, "Mismatch at chunk %zu, index %zu: expected %d, got %d\n", i, j, expected, chunk[j]);
              ret = -1;
              goto cleanup;
            }
        }
    }

  printf ("SUCCESS: %d inserts and reads, total %zu elements verified\n", NUM_INSERTS, total_inserted);

cleanup:
  if (n)
    {
      nsfslite_close (n);
    }
  free (chunk);
  return ret;
}
