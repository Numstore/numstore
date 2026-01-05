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
 *   Test implementation for ARIES crash recovery without checkpoints.
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

TEST_disabled (TT_UNIT, aries_crash_before_commit)
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

  // NORMAL PROCESSING
  {
    struct txn tx1;
    test_err_t_wrap (pgr_begin_txn (&tx1, p, &e), &e);

    // Make a bunch of changes to ensure
    // at least one UPDATE is written to the wal
    for (int i = 0; i < 5; ++i)
      {
        // TID1
        page_h dl_page = page_h_create ();
        test_fail_if (pgr_new (&dl_page, p, &tx1, PG_DATA_LIST, &e));
        dl_make_valid (page_h_w (&dl_page));
        test_fail_if (pgr_release (p, &dl_page, PG_DATA_LIST, &e));
      }

    test_err_t_wrap (pgr_flush_wall (p, &e), &e);
  }

  // REOPEN
  test_fail_if (pgr_crash (p, &e));
  p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_fail_if_null (p);

  // VALIDATION
  {
    page_h pg = page_h_create ();

    // Root node was committed
    test_err_t_wrap (pgr_get (&pg, PG_ROOT_NODE, 0, p, &e), &e);
    test_assert_int_equal (rn_get_first_tmbst (page_h_ro (&pg)), 1);
    test_assert_int_equal (rn_get_master_lsn (page_h_ro (&pg)), 0);
    pgr_release (p, &pg, PG_ROOT_NODE, &e);

    // The data lists weren't commited (and therefore tombstones)
    for (int i = 1; i < 6; ++i)
      {
        // TID1
        test_err_t_wrap (pgr_get (&pg, PG_TOMBSTONE, i, p, &e), &e);
        test_assert_int_equal (tmbst_get_next (page_h_ro (&pg)), i + 1);
        pgr_release (p, &pg, PG_TOMBSTONE, &e);
      }
  }

  test_err_t_wrap (pgr_close (p, &e), &e);

  test_err_t_wrap (tp_free (tp, &e), &e);
  lockt_destroy (&lt);
}

TEST_disabled (TT_UNIT, aries_crash_before_commit_multiple)
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

  struct txn tx1, tx2;

  // NORMAL PROCESSING
  {
    test_err_t_wrap (pgr_begin_txn (&tx1, p, &e), &e);

    // Make a bunch of changes to ensure
    // at least one UPDATE is written to the wal
    for (int i = 0; i < 5; ++i)
      {
        // TID1
        page_h dl_page = page_h_create ();
        test_fail_if (pgr_new (&dl_page, p, &tx1, PG_DATA_LIST, &e));
        dl_make_valid (page_h_w (&dl_page));
        test_fail_if (pgr_release (p, &dl_page, PG_DATA_LIST, &e));
      }

    test_err_t_wrap (pgr_flush_wall (p, &e), &e);
  }

  // REOPEN
  test_fail_if (pgr_crash (p, &e));
  p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_fail_if_null (p);

  // NORMAL PROCESSING
  {
    test_err_t_wrap (pgr_begin_txn (&tx2, p, &e), &e);
    test_assert (tx1.tid != tx2.tid); // ARIES found the maximum transaction

    // Make a bunch of changes to ensure
    // at least one UPDATE is written to the wal
    for (int i = 0; i < 5; ++i)
      {
        // TID1
        page_h dl_page = page_h_create ();
        test_fail_if (pgr_new (&dl_page, p, &tx2, PG_DATA_LIST, &e));
        dl_make_valid (page_h_w (&dl_page));
        test_fail_if (pgr_release (p, &dl_page, PG_DATA_LIST, &e));
      }

    test_err_t_wrap (pgr_flush_wall (p, &e), &e);
  }

  // REOPEN
  test_fail_if (pgr_crash (p, &e));
  p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_fail_if_null (p);

  // VALIDATION
  {
    page_h pg = page_h_create ();

    // Root node was committed
    test_err_t_wrap (pgr_get (&pg, PG_ROOT_NODE, 0, p, &e), &e);
    test_assert_int_equal (rn_get_first_tmbst (page_h_ro (&pg)), 1);
    test_assert_int_equal (rn_get_master_lsn (page_h_ro (&pg)), 0);
    pgr_release (p, &pg, PG_ROOT_NODE, &e);

    // The data lists weren't commited (and therefore tombstones)
    for (int i = 1; i < 6; ++i)
      {
        // TID1
        test_err_t_wrap (pgr_get (&pg, PG_TOMBSTONE, i, p, &e), &e);
        test_assert_int_equal (tmbst_get_next (page_h_ro (&pg)), i + 1);
        pgr_release (p, &pg, PG_TOMBSTONE, &e);
      }
  }

  test_err_t_wrap (pgr_close (p, &e), &e);

  test_err_t_wrap (tp_free (tp, &e), &e);
  lockt_destroy (&lt);
}

TEST (TT_UNIT, aries_crash_after_commit_before_end)
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

  u8 data[5][DL_DATA_SIZE];
  rand_bytes (data, DL_DATA_SIZE * 5);

  // NORMAL PROCESSING
  {
    struct txn tx1, tx;
    test_err_t_wrap (pgr_begin_txn (&tx1, p, &e), &e);
    test_err_t_wrap (pgr_begin_txn (&tx, p, &e), &e);

    // Make a bunch of changes to ensure
    // at least one UPDATE is written to the wal
    for (int i = 0; i < 5; ++i)
      {
        // TID1
        page_h dl_page = page_h_create ();
        test_fail_if (pgr_new (&dl_page, p, &tx1, PG_DATA_LIST, &e));
        dl_set_data (page_h_w (&dl_page), (struct dl_data){ .data = data[i], .blen = DL_DATA_SIZE });
        dl_set_prev (page_h_w (&dl_page), i + 20);
        dl_set_next (page_h_w (&dl_page), i + 100);
        test_fail_if (pgr_release (p, &dl_page, PG_DATA_LIST, &e));
      }

    test_err_t_wrap (pgr_commit (p, &tx1, &e), &e);
  }

  // REOPEN
  test_fail_if (pgr_crash (p, &e));
  p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_fail_if_null (p);

  // VALIDATION
  {
    page_h pg = page_h_create ();

    // Root node was committed
    test_err_t_wrap (pgr_get (&pg, PG_ROOT_NODE, 0, p, &e), &e);
    test_assert_int_equal (rn_get_first_tmbst (page_h_ro (&pg)), 6);
    test_assert_int_equal (rn_get_master_lsn (page_h_ro (&pg)), 0);
    pgr_release (p, &pg, PG_ROOT_NODE, &e);

    // The data lists was commited
    for (int i = 1; i < 6; ++i)
      {
        // TID1
        test_err_t_wrap (pgr_get (&pg, PG_DATA_LIST, i, p, &e), &e);
        test_assert_memequal (dl_get_data (page_h_ro (&pg)), data[i - 1], DL_DATA_SIZE);
        test_assert_int_equal (dl_get_prev (page_h_ro (&pg)), (i - 1) + 20);
        test_assert_int_equal (dl_get_next (page_h_ro (&pg)), (i - 1) + 100);
        pgr_release (p, &pg, PG_DATA_LIST, &e);
      }
  }

  test_err_t_wrap (pgr_close (p, &e), &e);

  test_err_t_wrap (tp_free (tp, &e), &e);
  lockt_destroy (&lt);
}

TEST (TT_UNIT, aries_crash_after_commit_before_end_multiple)
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

  u8 data[10][DL_DATA_SIZE];
  rand_bytes (data, DL_DATA_SIZE * 10);

  struct txn tx1, tx2;

  // NORMAL PROCESSING
  {
    test_err_t_wrap (pgr_begin_txn (&tx1, p, &e), &e);

    // Make a bunch of changes to ensure
    // at least one UPDATE is written to the wal
    for (int i = 0; i < 5; ++i)
      {
        // TID1
        page_h dl_page = page_h_create ();
        test_fail_if (pgr_new (&dl_page, p, &tx1, PG_DATA_LIST, &e));
        dl_set_data (page_h_w (&dl_page), (struct dl_data){ .data = data[i], .blen = DL_DATA_SIZE });
        dl_set_prev (page_h_w (&dl_page), i + 20);
        dl_set_next (page_h_w (&dl_page), i + 100);
        test_fail_if (pgr_release (p, &dl_page, PG_DATA_LIST, &e));
      }

    test_err_t_wrap (pgr_commit (p, &tx1, &e), &e);
  }

  // REOPEN
  test_fail_if (pgr_crash (p, &e));
  p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_fail_if_null (p);

  // NORMAL PROCESSING
  {
    test_err_t_wrap (pgr_begin_txn (&tx2, p, &e), &e);
    test_assert (tx2.tid > tx1.tid); // ARIES Found max page

    // Make a bunch of changes to ensure
    // at least one UPDATE is written to the wal
    for (int i = 5; i < 10; ++i)
      {
        // TID1
        page_h dl_page = page_h_create ();
        test_fail_if (pgr_new (&dl_page, p, &tx2, PG_DATA_LIST, &e));
        dl_set_data (page_h_w (&dl_page), (struct dl_data){ .data = data[i], .blen = DL_DATA_SIZE });
        dl_set_prev (page_h_w (&dl_page), i + 20);
        dl_set_next (page_h_w (&dl_page), i + 100);
        test_fail_if (pgr_release (p, &dl_page, PG_DATA_LIST, &e));
      }

    test_err_t_wrap (pgr_commit (p, &tx2, &e), &e);
  }

  // REOPEN
  test_fail_if (pgr_crash (p, &e));
  p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_fail_if_null (p);

  // VALIDATION
  {
    page_h pg = page_h_create ();

    // Root node was committed
    test_err_t_wrap (pgr_get (&pg, PG_ROOT_NODE, 0, p, &e), &e);
    test_assert_int_equal (rn_get_first_tmbst (page_h_ro (&pg)), 11);
    test_assert_int_equal (rn_get_master_lsn (page_h_ro (&pg)), 0);
    pgr_release (p, &pg, PG_ROOT_NODE, &e);

    // The data lists was commited
    for (int i = 1; i < 6; ++i)
      {
        // TID1
        test_err_t_wrap (pgr_get (&pg, PG_DATA_LIST, i, p, &e), &e);
        test_assert_memequal (dl_get_data (page_h_ro (&pg)), data[i - 1], DL_DATA_SIZE);
        test_assert_int_equal (dl_get_prev (page_h_ro (&pg)), (i - 1) + 20);
        test_assert_int_equal (dl_get_next (page_h_ro (&pg)), (i - 1) + 100);
        pgr_release (p, &pg, PG_DATA_LIST, &e);
      }

    // The data lists was commited
    for (int i = 6; i < 11; ++i)
      {
        // TID1
        test_err_t_wrap (pgr_get (&pg, PG_DATA_LIST, i, p, &e), &e);
        test_assert_memequal (dl_get_data (page_h_ro (&pg)), data[i - 1], DL_DATA_SIZE);
        test_assert_int_equal (dl_get_prev (page_h_ro (&pg)), (i - 1) + 20);
        test_assert_int_equal (dl_get_next (page_h_ro (&pg)), (i - 1) + 100);
        pgr_release (p, &pg, PG_DATA_LIST, &e);
      }
  }

  test_err_t_wrap (pgr_close (p, &e), &e);

  test_err_t_wrap (tp_free (tp, &e), &e);
  lockt_destroy (&lt);
}

TEST (TT_UNIT, aries_crash_after_commit_before_end_multiple_second_no_commit)
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

  u8 data[10][DL_DATA_SIZE];
  rand_bytes (data, DL_DATA_SIZE * 10);

  struct txn tx1, tx2;

  // NORMAL PROCESSING
  {
    test_err_t_wrap (pgr_begin_txn (&tx1, p, &e), &e);

    // Make a bunch of changes to ensure
    // at least one UPDATE is written to the wal
    for (int i = 0; i < 5; ++i)
      {
        // TID1
        page_h dl_page = page_h_create ();
        test_fail_if (pgr_new (&dl_page, p, &tx1, PG_DATA_LIST, &e));
        dl_set_data (page_h_w (&dl_page), (struct dl_data){ .data = data[i], .blen = DL_DATA_SIZE });
        dl_set_prev (page_h_w (&dl_page), i + 20);
        dl_set_next (page_h_w (&dl_page), i + 100);
        test_fail_if (pgr_release (p, &dl_page, PG_DATA_LIST, &e));
      }

    test_err_t_wrap (pgr_commit (p, &tx1, &e), &e);
  }

  // REOPEN
  test_fail_if (pgr_crash (p, &e));
  p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_fail_if_null (p);

  // NORMAL PROCESSING
  {
    test_err_t_wrap (pgr_begin_txn (&tx2, p, &e), &e);
    test_assert (tx2.tid > tx1.tid); // ARIES Found max page

    // Make a bunch of changes to ensure
    // at least one UPDATE is written to the wal
    for (int i = 5; i < 10; ++i)
      {
        // TID1
        page_h dl_page = page_h_create ();
        test_fail_if (pgr_new (&dl_page, p, &tx2, PG_DATA_LIST, &e));
        dl_set_data (page_h_w (&dl_page), (struct dl_data){ .data = data[i], .blen = DL_DATA_SIZE });
        dl_set_prev (page_h_w (&dl_page), i + 20);
        dl_set_next (page_h_w (&dl_page), i + 100);
        test_fail_if (pgr_release (p, &dl_page, PG_DATA_LIST, &e));
      }
  }

  // REOPEN
  test_fail_if (pgr_crash (p, &e));
  p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_fail_if_null (p);
  return;

  // VALIDATION
  {
    page_h pg = page_h_create ();

    // Root node was committed
    test_err_t_wrap (pgr_get (&pg, PG_ROOT_NODE, 0, p, &e), &e);
    test_assert_int_equal (rn_get_first_tmbst (page_h_ro (&pg)), 6);
    test_assert_int_equal (rn_get_master_lsn (page_h_ro (&pg)), 0);
    pgr_release (p, &pg, PG_ROOT_NODE, &e);

    // The data lists was commited
    for (int i = 1; i < 6; ++i)
      {
        // TID1
        test_err_t_wrap (pgr_get (&pg, PG_DATA_LIST, i, p, &e), &e);
        test_assert_memequal (dl_get_data (page_h_ro (&pg)), data[i - 1], DL_DATA_SIZE);
        test_assert_int_equal (dl_get_prev (page_h_ro (&pg)), (i - 1) + 20);
        test_assert_int_equal (dl_get_next (page_h_ro (&pg)), (i - 1) + 100);
        pgr_release (p, &pg, PG_DATA_LIST, &e);
      }

    // The data lists was commited
    for (int i = 6; i < 11; ++i)
      {
        // TID2
        test_err_t_wrap (pgr_get (&pg, PG_TOMBSTONE, i, p, &e), &e);
        test_assert_int_equal (tmbst_get_next (page_h_ro (&pg)), i + 1);
        pgr_release (p, &pg, PG_TOMBSTONE, &e);
      }
  }

  test_err_t_wrap (pgr_close (p, &e), &e);

  test_err_t_wrap (tp_free (tp, &e), &e);
  lockt_destroy (&lt);
}
#endif
#endif
