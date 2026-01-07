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
 * Example 2: Crash Recovery Test
 *
 * Demonstrates nsfslite's ability to recover data after an unexpected crash.
 *
 * Usage:
 *   ./example2_crash_recovery          # Run initial insert and simulate crash
 *   ./example2_crash_recovery recover  # Recover and verify data after crash
 *
 * This example tests the write-ahead log (WAL) recovery mechanism by:
 * 1. First run: Inserts data and aborts without closing (simulates crash)
 * 2. Second run: Reopens database and verifies all data was recovered
 */

#include "nsfslite.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define N_ELEMS 200000

int
main (int argc, char **argv)
{
  int ret = 0;
  nsfslite *n = NULL;
  int *data = int_range (N_ELEMS);
  int *read_data = int_random (N_ELEMS);

  if (argc > 1 && strcmp (argv[1], "recover") == 0)
    {
      // OPEN
      CHECKN ((n = nsfslite_open ("test2.db", "test2.wal")));

      // GET ID
      int64_t id = nsfslite_get_id (n, "data");
      CHECK (id);

      // READ
      struct nsfslite_stride rstride = { .bstart = 0, .stride = 1, .nelems = N_ELEMS };
      CHECK (nsfslite_read (n, id, read_data, sizeof (int), rstride));

      // COMPARE
      COMPARE (data, read_data, N_ELEMS);

      printf ("SUCCESS: All %d elements recovered after crash\n", N_ELEMS);
    }
  else
    {
      unlink ("test2.db");
      unlink ("test2.wal");

      // OPEN
      CHECKN ((n = nsfslite_open ("test2.db", "test2.wal")));

      // NEW VARIABLE
      int64_t id = nsfslite_new (n, NULL, "data");
      CHECK (id);

      // INSERT
      CHECK (nsfslite_insert (n, id, NULL, data, 0, sizeof (int), N_ELEMS));

      printf ("Inserted %d elements, crashing without close...\n", N_ELEMS);
      abort ();
    }

cleanup:
  if (n)
    {
      nsfslite_close (n);
    }
  free (data);
  free (read_data);
  return ret;
}
