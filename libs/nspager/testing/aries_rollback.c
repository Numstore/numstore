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
 *   Test implementation for ARIES transaction rollback and undo operations.
 */

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/core/random.h>
#include <numstore/pager.h>
#include <numstore/pager/data_list.h>
#include <numstore/pager/inner_node.h>
#include <numstore/pager/page.h>
#include <numstore/pager/page_h.h>
#include <numstore/pager/root_node.h>
#include <numstore/pager/tombstone.h>
#include <numstore/pager/txn_table.h>
#include <numstore/test/page_fixture.h>
#include <numstore/test/testing.h>
#include <numstore/test/testing_test.h>

#include <config.h>
#include <wal.h>

#ifndef DUMB_PAGER

#ifndef NTEST

TEST (TT_UNIT, aries_rollback_basic)
{
  error e = error_create ();

  struct lockt lt;
  test_err_t_wrap (lockt_init (&lt, &e), &e);

  struct thread_pool *tp = tp_open (&e);
  test_fail_if_null (tp);

  test_fail_if (i_remove_quiet ("test.db", &e));
  test_fail_if (i_remove_quiet ("test.wal", &e));

  struct pager *p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_fail_if_null (p);

  // Create transaction and make changes
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, p, &e), &e);

  // Create some data pages
  for (int i = 0; i < 3; ++i)
    {
      page_h dl_page = page_h_create ();
      test_fail_if (pgr_new (&dl_page, p, &tx, PG_DATA_LIST, &e));
      dl_make_valid (page_h_w (&dl_page));
      test_fail_if (pgr_release (p, &dl_page, PG_DATA_LIST, &e));
    }

  test_err_t_wrap (pgr_flush_wall (p, &e), &e);

  // Rollback the transaction
  test_err_t_wrap (pgr_rollback (p, &tx, 0, &e), &e);

  // Verify pages are tombstones after rollback
  page_h pg = page_h_create ();
  for (int i = 1; i < 4; ++i)
    {
      test_err_t_wrap (pgr_get (&pg, PG_TOMBSTONE, i, p, &e), &e);
      test_assert_int_equal (tmbst_get_next (page_h_ro (&pg)), i + 1);
      pgr_release (p, &pg, PG_TOMBSTONE, &e);
    }

  test_err_t_wrap (pgr_close (p, &e), &e);

  test_err_t_wrap (tp_free (tp, &e), &e);
  lockt_destroy (&lt);
}

TEST (TT_UNIT, aries_rollback_multiple_updates)
{
  error e = error_create ();

  struct lockt lt;
  test_err_t_wrap (lockt_init (&lt, &e), &e);

  struct thread_pool *tp = tp_open (&e);
  test_fail_if_null (tp);

  test_fail_if (i_remove_quiet ("test.db", &e));
  test_fail_if (i_remove_quiet ("test.wal", &e));

  struct pager *p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_fail_if_null (p);

  // Create and commit initial data
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, p, &e), &e);

  page_h dl_page = page_h_create ();
  test_fail_if (pgr_new (&dl_page, p, &tx, PG_DATA_LIST, &e));
  dl_make_valid (page_h_w (&dl_page));

  // Write initial data
  u8 initial_data[DL_DATA_SIZE];
  i_memset (initial_data, 0xAA, DL_DATA_SIZE);
  dl_set_data (page_h_w (&dl_page), (struct dl_data){ .data = initial_data, .blen = DL_DATA_SIZE });

  pgno pgno1 = page_h_ro (&dl_page)->pg;
  test_fail_if (pgr_release (p, &dl_page, PG_DATA_LIST, &e));
  test_err_t_wrap (pgr_commit (p, &tx, &e), &e);

  // Start new transaction and make multiple updates
  struct txn tx2;
  test_err_t_wrap (pgr_begin_txn (&tx2, p, &e), &e);

  // Update 1
  test_err_t_wrap (pgr_get_writable (&dl_page, &tx2, PG_DATA_LIST, pgno1, p, &e), &e);
  u8 update1_data[DL_DATA_SIZE];
  i_memset (update1_data, 0xBB, DL_DATA_SIZE);
  dl_set_data (page_h_w (&dl_page), (struct dl_data){ .data = update1_data, .blen = DL_DATA_SIZE });
  test_fail_if (pgr_release (p, &dl_page, PG_DATA_LIST, &e));

  // Update 2
  test_err_t_wrap (pgr_get_writable (&dl_page, &tx2, PG_DATA_LIST, pgno1, p, &e), &e);
  u8 update2_data[DL_DATA_SIZE];
  i_memset (update2_data, 0xCC, DL_DATA_SIZE);
  dl_set_data (page_h_w (&dl_page), (struct dl_data){ .data = update2_data, .blen = DL_DATA_SIZE });
  test_fail_if (pgr_release (p, &dl_page, PG_DATA_LIST, &e));

  // Update 3
  test_err_t_wrap (pgr_get_writable (&dl_page, &tx2, PG_DATA_LIST, pgno1, p, &e), &e);
  u8 update3_data[DL_DATA_SIZE];
  i_memset (update3_data, 0xDD, DL_DATA_SIZE);
  dl_set_data (page_h_w (&dl_page), (struct dl_data){ .data = update3_data, .blen = DL_DATA_SIZE });
  test_fail_if (pgr_release (p, &dl_page, PG_DATA_LIST, &e));

  test_err_t_wrap (pgr_flush_wall (p, &e), &e);

  // Rollback all updates
  test_err_t_wrap (pgr_rollback (p, &tx2, 0, &e), &e);

  // Verify data is back to initial state
  test_err_t_wrap (pgr_get (&dl_page, PG_DATA_LIST, pgno1, p, &e), &e);
  test_assert_memequal (dl_get_data (page_h_ro (&dl_page)), initial_data, DL_DATA_SIZE);
  pgr_release (p, &dl_page, PG_DATA_LIST, &e);

  test_err_t_wrap (pgr_close (p, &e), &e);

  test_err_t_wrap (tp_free (tp, &e), &e);
  lockt_destroy (&lt);
}

TEST_disabled (TT_UNIT, aries_rollback_with_crash_recovery)
{
  error e = error_create ();

  struct lockt lt;
  test_err_t_wrap (lockt_init (&lt, &e), &e);

  struct thread_pool *tp = tp_open (&e);
  test_fail_if_null (tp);

  test_fail_if (i_remove_quiet ("test.db", &e));
  test_fail_if (i_remove_quiet ("test.wal", &e));

  struct pager *p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_fail_if_null (p);

  // Transaction 1: commit data
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, p, &e), &e);

  page_h dl_page = page_h_create ();
  test_fail_if (pgr_new (&dl_page, p, &tx, PG_DATA_LIST, &e));

  u8 committed_data[DL_DATA_SIZE];
  i_memset (committed_data, 0xAA, DL_DATA_SIZE);
  dl_set_data (page_h_w (&dl_page), (struct dl_data){ .data = committed_data, .blen = DL_DATA_SIZE });

  pgno pgno1 = page_h_ro (&dl_page)->pg;
  test_fail_if (pgr_release (p, &dl_page, PG_DATA_LIST, &e));
  test_err_t_wrap (pgr_commit (p, &tx, &e), &e);

  // Transaction 2: make changes but don't commit or rollback (will be rolled back on crash)
  struct txn tx2;
  test_err_t_wrap (pgr_begin_txn (&tx2, p, &e), &e);

  test_err_t_wrap (pgr_get_writable (&dl_page, &tx2, PG_DATA_LIST, pgno1, p, &e), &e);
  u8 uncommitted_data[DL_DATA_SIZE];
  i_memset (uncommitted_data, 0xBB, DL_DATA_SIZE);
  dl_set_data (page_h_w (&dl_page), (struct dl_data){ .data = uncommitted_data, .blen = DL_DATA_SIZE });
  test_fail_if (pgr_release (p, &dl_page, PG_DATA_LIST, &e));

  test_err_t_wrap (pgr_flush_wall (p, &e), &e);

  // Crash without commit or rollback
  test_fail_if (pgr_crash (p, &e));
  p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_fail_if_null (p);

  // Verify data is back to committed state (&tx2 was rolled back during recovery)
  test_err_t_wrap (pgr_get (&dl_page, PG_DATA_LIST, pgno1, p, &e), &e);
  test_assert_memequal (dl_get_data (page_h_ro (&dl_page)), committed_data, DL_DATA_SIZE);
  pgr_release (p, &dl_page, PG_DATA_LIST, &e);

  test_err_t_wrap (pgr_close (p, &e), &e);

  test_err_t_wrap (tp_free (tp, &e), &e);
  lockt_destroy (&lt);
}

TEST_disabled (TT_UNIT, aries_rollback_clr_not_undone)
{
  error e = error_create ();

  struct lockt lt;
  test_err_t_wrap (lockt_init (&lt, &e), &e);

  struct thread_pool *tp = tp_open (&e);
  test_fail_if_null (tp);

  test_fail_if (i_remove_quiet ("test.db", &e));
  test_fail_if (i_remove_quiet ("test.wal", &e));

  struct pager *p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_fail_if_null (p);

  // Create and commit initial page
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, p, &e), &e);

  page_h dl_page = page_h_create ();
  test_fail_if (pgr_new (&dl_page, p, &tx, PG_DATA_LIST, &e));
  dl_make_valid (page_h_w (&dl_page));

  u8 initial_data[DL_DATA_SIZE];
  i_memset (initial_data, 0xAA, DL_DATA_SIZE);
  dl_set_data (page_h_w (&dl_page), (struct dl_data){ .data = initial_data, .blen = DL_DATA_SIZE });

  pgno pgno1 = page_h_ro (&dl_page)->pg;
  test_fail_if (pgr_release (p, &dl_page, PG_DATA_LIST, &e));
  test_err_t_wrap (pgr_commit (p, &tx, &e), &e);

  // Transaction 2: make update then rollback (generates CLRs)
  struct txn tx2;
  test_err_t_wrap (pgr_begin_txn (&tx2, p, &e), &e);

  test_err_t_wrap (pgr_get_writable (&dl_page, &tx2, PG_DATA_LIST, pgno1, p, &e), &e);
  u8 temp_data[DL_DATA_SIZE];
  i_memset (temp_data, 0xBB, DL_DATA_SIZE);
  dl_set_data (page_h_w (&dl_page), (struct dl_data){ .data = temp_data, .blen = DL_DATA_SIZE });
  test_fail_if (pgr_release (p, &dl_page, PG_DATA_LIST, &e));

  test_err_t_wrap (pgr_flush_wall (p, &e), &e);

  // Rollback &tx2 (this writes CLRs)
  test_err_t_wrap (pgr_rollback (p, &tx2, 0, &e), &e);

  // Verify data is back to initial
  test_err_t_wrap (pgr_get (&dl_page, PG_DATA_LIST, pgno1, p, &e), &e);
  test_assert_memequal (dl_get_data (page_h_ro (&dl_page)), initial_data, DL_DATA_SIZE);
  pgr_release (p, &dl_page, PG_DATA_LIST, &e);

  // Crash and recover to ensure CLRs are handled correctly
  test_fail_if (pgr_crash (p, &e));
  p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_fail_if_null (p);

  // Verify data is still initial (CLRs were not undone during recovery)
  test_err_t_wrap (pgr_get (&dl_page, PG_DATA_LIST, pgno1, p, &e), &e);
  test_assert_memequal (dl_get_data (page_h_ro (&dl_page)), initial_data, DL_DATA_SIZE);
  pgr_release (p, &dl_page, PG_DATA_LIST, &e);

  test_err_t_wrap (pgr_close (p, &e), &e);

  test_err_t_wrap (tp_free (tp, &e), &e);
  lockt_destroy (&lt);
}

#endif
#endif
