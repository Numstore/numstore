/*
 * Copyright 2025 Theo Lincke
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
 * Description:
 *   Scratch/example program demonstrating database creation.
 */

// numstore
#include <numstore/nslib/rptree/rptree.h>

#include "paging.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// system
int
main (void)
{
  error e;
  pager *p;
  stxid t;
  rptree *r;
  u32 write_data[20480];
  u32 read_data[2048];

  // Initialize
  {
    e = error_create ();
    i_remove_quiet ("test.db", &e);
    i_remove_quiet ("test.wal", &e);

    p = pgr_open ("test.db", "test.wal", &e);

    t = pgr_begin_txn (p, &e); // TODO - this is not used - should warn or
    // do something on pager close

    /* Create */
    r = rpt_new (p, &e);

    for (int i = 0; i < 20480; ++i)
      {
        write_data[i] = i;
      }
  }

  // BEGIN TRANSACTION
  t = pgr_begin_txn (p, &e);

  // INSERT 0 write_data
  rpt_insert ((u8 *)write_data, t, 4, sizeof (write_data), 0, r, &e);

  // INSERT 3 write_data
  rpt_insert ((u8 *)write_data, t, 4, sizeof (write_data), 3, r, &e);

  // Before:  0, 1, 2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  rpt_remove (NULL, t, 4, 1000, 5, 0, 2, r, &e);

  pgr_close (p, &e);
  panic ("Done");

  // After:   0, 1, 2, 0, 1, 3, 5, 7, 9, 11, 13, 15
  pgr_commit (p, t, &e);

  rpt_read ((u8 *)read_data, 4, 100, 0, 0, 1, r, &e);

  // Read:   0, 1, 2, 0, 1, 3, 5, 7, 9, 11, 13, 15
  for (int i = 0; i < 100; ++i)
    {
      printf ("%d ", read_data[i]);
    }
  printf ("\n");

  rpt_close (r, &e);
  pgr_close (p, &e);
}
