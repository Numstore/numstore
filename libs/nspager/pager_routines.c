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
 *   Implements pager_routines.h. High-level utility routines simplifying common pager operations.
 */

#include <numstore/pager/pager_routines.h>

#include <numstore/core/error.h>
#include <numstore/core/macros.h>
#include <numstore/core/math.h>
#include <numstore/intf/stdlib.h>
#include <numstore/intf/types.h>
#include <numstore/pager.h>
#include <numstore/pager/data_list.h>
#include <numstore/pager/page_delegate.h>
#include <numstore/pager/page_h.h>
#include <numstore/test/page_fixture.h>
#include <numstore/test/testing.h>

/////////////////////////////////////////
/// DELETING NODES

err_t
pgr_dlgt_delete_and_fill_next (page_h *cur, page_h *next, struct txn *tx, struct pager *p, error *e)
{
  ASSERT (cur);
  ASSERT (next);
  ASSERT (tx);
  ASSERT (cur->mode != PHM_NONE);
  ASSERT (next->mode != PHM_NONE);
  ASSERT (dlgt_valid_neighbors (page_h_ro (cur), page_h_ro (next)));

  page_h next_next = page_h_create ();
  if (pgr_dlgt_get_next (next, &next_next, tx, p, e))
    {
      return e->cause_code;
    }

  if (pgr_delete_and_release (p, tx, next, e))
    {
      return e->cause_code;
    }

  err_t_wrap (pgr_maybe_make_writable (p, tx, cur, e), e);
  dlgt_link (page_h_w (cur), page_h_w_or_null (&next_next));
  page_h_xfer_ownership_ptr (next, &next_next);

  return SUCCESS;
}

TEST (TT_UNIT, pgr_dlgt_delete_and_fill_next)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  TEST_CASE ("data_list")
  {
    struct page_tree_builder builder = in5dl (f.p, &tx, 5, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE);
    test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

    page_h *root = &builder.root.out;
    page_h *n1 = &builder.root.inner.children[0].out;
    page_h *n2 = &builder.root.inner.children[1].out;
    page_h *n3 = &builder.root.inner.children[2].out;
    page_h *n4 = &builder.root.inner.children[3].out;
    page_h *n5 = &builder.root.inner.children[4].out;

    pgno n1pg = page_h_pgno (n1);
    pgno n2pg = page_h_pgno (n2);
    pgno n3pg = page_h_pgno (n3);
    pgno n4pg = page_h_pgno (n4);
    pgno n5pg = page_h_pgno (n5);

    test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n1, PG_DATA_LIST, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n4, PG_DATA_LIST, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n5, PG_DATA_LIST, &f.e), &f.e);

    test_err_t_wrap (pgr_dlgt_delete_and_fill_next (n2, n3, &tx, f.p, &f.e), &f.e);
    test_assert_type_equal (page_h_pgno (n3), n4pg, pgno, PRpgno);
    test_err_t_wrap (pgr_dlgt_delete_and_fill_next (n2, n3, &tx, f.p, &f.e), &f.e);
    test_assert_type_equal (page_h_pgno (n3), n5pg, pgno, PRpgno);
    test_err_t_wrap (pgr_dlgt_delete_and_fill_next (n2, n3, &tx, f.p, &f.e), &f.e);
    test_assert_type_equal (n3->mode, PHM_NONE, pgno, PRpgno);

    test_err_t_wrap (pgr_release (f.p, n2, PG_DATA_LIST, &f.e), &f.e);

    test_err_t_wrap (pgr_get (n1, PG_DATA_LIST, n1pg, f.p, &f.e), &f.e);
    test_err_t_wrap (pgr_get (n2, PG_DATA_LIST, n2pg, f.p, &f.e), &f.e);
    test_err_t_wrap (pgr_get (n3, PG_TOMBSTONE, n3pg, f.p, &f.e), &f.e);
    test_err_t_wrap (pgr_get (n4, PG_TOMBSTONE, n4pg, f.p, &f.e), &f.e);
    test_err_t_wrap (pgr_get (n5, PG_TOMBSTONE, n5pg, f.p, &f.e), &f.e);

    test_assert (dlgt_valid_neighbors (NULL, page_h_ro (n1)));
    test_assert (dlgt_valid_neighbors (page_h_ro (n1), page_h_ro (n2)));
    test_assert (dlgt_valid_neighbors (page_h_ro (n2), NULL));

    test_err_t_wrap (pgr_release (f.p, n1, PG_DATA_LIST, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n2, PG_DATA_LIST, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n3, PG_TOMBSTONE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n4, PG_TOMBSTONE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n5, PG_TOMBSTONE, &f.e), &f.e);
  }

  TEST_CASE ("inner_node")
  {
    struct page_tree_builder builder = in5in (f.p, &tx, 5, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS);
    test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

    page_h *root = &builder.root.out;
    page_h *n1 = &builder.root.inner.children[0].out;
    page_h *n2 = &builder.root.inner.children[1].out;
    page_h *n3 = &builder.root.inner.children[2].out;
    page_h *n4 = &builder.root.inner.children[3].out;
    page_h *n5 = &builder.root.inner.children[4].out;

    pgno n1pg = page_h_pgno (n1);
    pgno n2pg = page_h_pgno (n2);
    pgno n3pg = page_h_pgno (n3);
    pgno n4pg = page_h_pgno (n4);
    pgno n5pg = page_h_pgno (n5);

    test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n1, PG_INNER_NODE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n4, PG_INNER_NODE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n5, PG_INNER_NODE, &f.e), &f.e);

    test_err_t_wrap (pgr_dlgt_delete_and_fill_next (n2, n3, &tx, f.p, &f.e), &f.e);
    test_assert_type_equal (page_h_pgno (n3), n4pg, pgno, PRpgno);
    test_err_t_wrap (pgr_dlgt_delete_and_fill_next (n2, n3, &tx, f.p, &f.e), &f.e);
    test_assert_type_equal (page_h_pgno (n3), n5pg, pgno, PRpgno);
    test_err_t_wrap (pgr_dlgt_delete_and_fill_next (n2, n3, &tx, f.p, &f.e), &f.e);
    test_assert_type_equal (n3->mode, PHM_NONE, pgno, PRpgno);

    test_err_t_wrap (pgr_release (f.p, n2, PG_INNER_NODE, &f.e), &f.e);

    test_err_t_wrap (pgr_get (n1, PG_INNER_NODE, n1pg, f.p, &f.e), &f.e);
    test_err_t_wrap (pgr_get (n2, PG_INNER_NODE, n2pg, f.p, &f.e), &f.e);
    test_err_t_wrap (pgr_get (n3, PG_TOMBSTONE, n3pg, f.p, &f.e), &f.e);
    test_err_t_wrap (pgr_get (n4, PG_TOMBSTONE, n4pg, f.p, &f.e), &f.e);
    test_err_t_wrap (pgr_get (n5, PG_TOMBSTONE, n5pg, f.p, &f.e), &f.e);

    test_assert (dlgt_valid_neighbors (NULL, page_h_ro (n1)));
    test_assert (dlgt_valid_neighbors (page_h_ro (n1), page_h_ro (n2)));
    test_assert (dlgt_valid_neighbors (page_h_ro (n2), NULL));

    test_err_t_wrap (pgr_release (f.p, n1, PG_INNER_NODE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n2, PG_INNER_NODE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n3, PG_TOMBSTONE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n4, PG_TOMBSTONE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n5, PG_TOMBSTONE, &f.e), &f.e);
  }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

err_t
pgr_dlgt_delete_and_fill_prev (page_h *prev, page_h *cur, struct txn *tx, struct pager *p, error *e)
{
  ASSERT (prev);
  ASSERT (cur);
  ASSERT (tx);
  ASSERT (prev->mode != PHM_NONE);
  ASSERT (cur->mode != PHM_NONE);
  ASSERT (dlgt_valid_neighbors (page_h_ro (prev), page_h_ro (cur)));

  page_h prev_prev = page_h_create ();
  if (pgr_dlgt_get_prev (prev, &prev_prev, tx, p, e))
    {
      return e->cause_code;
    }

  if (pgr_delete_and_release (p, tx, prev, e))
    {
      return e->cause_code;
    }

  err_t_wrap (pgr_maybe_make_writable (p, tx, cur, e), e);
  dlgt_link (page_h_w_or_null (&prev_prev), page_h_w (cur));
  page_h_xfer_ownership_ptr (prev, &prev_prev);

  return SUCCESS;
}

TEST (TT_UNIT, pgr_dlgt_delete_and_fill_prev)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  TEST_CASE ("data_list")
  {
    struct page_tree_builder builder = in5dl (f.p, &tx, 5, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE);
    test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

    page_h *root = &builder.root.out;
    page_h *n1 = &builder.root.inner.children[0].out;
    page_h *n2 = &builder.root.inner.children[1].out;
    page_h *n3 = &builder.root.inner.children[2].out;
    page_h *n4 = &builder.root.inner.children[3].out;
    page_h *n5 = &builder.root.inner.children[4].out;

    pgno n1pg = page_h_pgno (n1);
    pgno n2pg = page_h_pgno (n2);
    pgno n3pg = page_h_pgno (n3);
    pgno n4pg = page_h_pgno (n4);
    pgno n5pg = page_h_pgno (n5);

    test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n1, PG_DATA_LIST, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n2, PG_DATA_LIST, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n5, PG_DATA_LIST, &f.e), &f.e);

    test_err_t_wrap (pgr_dlgt_delete_and_fill_prev (n3, n4, &tx, f.p, &f.e), &f.e);
    test_assert_type_equal (page_h_pgno (n3), n2pg, pgno, PRpgno);
    test_err_t_wrap (pgr_dlgt_delete_and_fill_prev (n3, n4, &tx, f.p, &f.e), &f.e);
    test_assert_type_equal (page_h_pgno (n3), n1pg, pgno, PRpgno);
    test_err_t_wrap (pgr_dlgt_delete_and_fill_prev (n3, n4, &tx, f.p, &f.e), &f.e);
    test_assert_type_equal (n3->mode, PHM_NONE, pgno, PRpgno);

    test_err_t_wrap (pgr_release (f.p, n4, PG_DATA_LIST, &f.e), &f.e);

    test_err_t_wrap (pgr_get (n1, PG_TOMBSTONE, n1pg, f.p, &f.e), &f.e);
    test_err_t_wrap (pgr_get (n2, PG_TOMBSTONE, n2pg, f.p, &f.e), &f.e);
    test_err_t_wrap (pgr_get (n3, PG_TOMBSTONE, n3pg, f.p, &f.e), &f.e);
    test_err_t_wrap (pgr_get (n4, PG_DATA_LIST, n4pg, f.p, &f.e), &f.e);
    test_err_t_wrap (pgr_get (n5, PG_DATA_LIST, n5pg, f.p, &f.e), &f.e);

    test_assert (dlgt_valid_neighbors (NULL, page_h_ro (n4)));
    test_assert (dlgt_valid_neighbors (page_h_ro (n4), page_h_ro (n5)));
    test_assert (dlgt_valid_neighbors (page_h_ro (n5), NULL));

    test_err_t_wrap (pgr_release (f.p, n1, PG_TOMBSTONE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n2, PG_TOMBSTONE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n3, PG_TOMBSTONE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n4, PG_DATA_LIST, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n5, PG_DATA_LIST, &f.e), &f.e);
  }

  TEST_CASE ("inner_node")
  {
    struct page_tree_builder builder = in5in (f.p, &tx, 5, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS);
    test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

    page_h *root = &builder.root.out;
    page_h *n1 = &builder.root.inner.children[0].out;
    page_h *n2 = &builder.root.inner.children[1].out;
    page_h *n3 = &builder.root.inner.children[2].out;
    page_h *n4 = &builder.root.inner.children[3].out;
    page_h *n5 = &builder.root.inner.children[4].out;

    pgno n1pg = page_h_pgno (n1);
    pgno n2pg = page_h_pgno (n2);
    pgno n3pg = page_h_pgno (n3);
    pgno n4pg = page_h_pgno (n4);
    pgno n5pg = page_h_pgno (n5);

    test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n1, PG_INNER_NODE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n2, PG_INNER_NODE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n5, PG_INNER_NODE, &f.e), &f.e);

    test_err_t_wrap (pgr_dlgt_delete_and_fill_prev (n3, n4, &tx, f.p, &f.e), &f.e);
    test_assert_type_equal (page_h_pgno (n3), n2pg, pgno, PRpgno);
    test_err_t_wrap (pgr_dlgt_delete_and_fill_prev (n3, n4, &tx, f.p, &f.e), &f.e);
    test_assert_type_equal (page_h_pgno (n3), n1pg, pgno, PRpgno);
    test_err_t_wrap (pgr_dlgt_delete_and_fill_prev (n3, n4, &tx, f.p, &f.e), &f.e);
    test_assert_type_equal (n3->mode, PHM_NONE, pgno, PRpgno);

    test_err_t_wrap (pgr_release (f.p, n4, PG_INNER_NODE, &f.e), &f.e);

    test_err_t_wrap (pgr_get (n1, PG_TOMBSTONE, n1pg, f.p, &f.e), &f.e);
    test_err_t_wrap (pgr_get (n2, PG_TOMBSTONE, n2pg, f.p, &f.e), &f.e);
    test_err_t_wrap (pgr_get (n3, PG_TOMBSTONE, n3pg, f.p, &f.e), &f.e);
    test_err_t_wrap (pgr_get (n4, PG_INNER_NODE, n4pg, f.p, &f.e), &f.e);
    test_err_t_wrap (pgr_get (n5, PG_INNER_NODE, n5pg, f.p, &f.e), &f.e);

    test_assert (dlgt_valid_neighbors (NULL, page_h_ro (n4)));
    test_assert (dlgt_valid_neighbors (page_h_ro (n4), page_h_ro (n5)));
    test_assert (dlgt_valid_neighbors (page_h_ro (n5), NULL));

    test_err_t_wrap (pgr_release (f.p, n1, PG_TOMBSTONE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n2, PG_TOMBSTONE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n3, PG_TOMBSTONE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n4, PG_INNER_NODE, &f.e), &f.e);
    test_err_t_wrap (pgr_release (f.p, n5, PG_INNER_NODE, &f.e), &f.e);
  }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

err_t
pgr_dlgt_delete_chain (page_h *cur, struct txn *tx, struct pager *p, error *e)
{
  ASSERT (cur);
  ASSERT (tx);
  ASSERT (cur->mode != PHM_NONE);

  page_h prev = page_h_create ();
  page_h next = page_h_create ();

  err_t_wrap (pgr_dlgt_get_next (cur, &next, tx, p, e), e);
  err_t_wrap (pgr_dlgt_get_prev (cur, &prev, tx, p, e), e);

  err_t_wrap (pgr_delete_and_release (p, tx, cur, e), e);

  while (next.mode != PHM_NONE)
    {
      page_h next_next = page_h_create ();
      err_t_wrap (pgr_dlgt_get_next (&next, &next_next, tx, p, e), e);
      err_t_wrap (pgr_delete_and_release (p, tx, &next, e), e);
      page_h_xfer_ownership_ptr (&next, &next_next);
    }

  while (prev.mode != PHM_NONE)
    {
      page_h prev_prev = page_h_create ();
      err_t_wrap (pgr_dlgt_get_prev (&prev, &prev_prev, tx, p, e), e);
      err_t_wrap (pgr_delete_and_release (p, tx, &prev, e), e);
      page_h_xfer_ownership_ptr (&prev, &prev_prev);
    }

  return SUCCESS;
}

TEST (TT_UNIT, pgr_dlgt_delete_chain)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  for (u32 i = 0; i < 5; ++i)
    {
      TEST_CASE ("delete_chain data list starting at: %d", i)
      {
        struct page_tree_builder builder = in5dl (f.p, &tx, 5, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_DATA_LIST, &f.e), &f.e);

        page_h *sut;

        switch (i)
          {
          case 0:
            {
              test_err_t_wrap (pgr_get (n1, PG_DATA_LIST, n1pg, f.p, &f.e), &f.e);
              sut = n1;
              break;
            }
          case 1:
            {
              test_err_t_wrap (pgr_get (n2, PG_DATA_LIST, n2pg, f.p, &f.e), &f.e);
              sut = n2;
              break;
            }
          case 2:
            {
              test_err_t_wrap (pgr_get (n3, PG_DATA_LIST, n3pg, f.p, &f.e), &f.e);
              sut = n3;
              break;
            }
          case 3:
            {
              test_err_t_wrap (pgr_get (n4, PG_DATA_LIST, n4pg, f.p, &f.e), &f.e);
              sut = n4;
              break;
            }
          case 4:
            {
              test_err_t_wrap (pgr_get (n5, PG_DATA_LIST, n5pg, f.p, &f.e), &f.e);
              sut = n5;
              break;
            }
          }

        test_err_t_wrap (pgr_dlgt_delete_chain (sut, &tx, f.p, &f.e), &f.e);

        test_assert_int_equal (n1->mode, PHM_NONE);
        test_assert_int_equal (n2->mode, PHM_NONE);
        test_assert_int_equal (n3->mode, PHM_NONE);
        test_assert_int_equal (n4->mode, PHM_NONE);
        test_assert_int_equal (n5->mode, PHM_NONE);

        test_err_t_wrap (pgr_get (n1, PG_TOMBSTONE, n1pg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_get (n2, PG_TOMBSTONE, n2pg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_get (n3, PG_TOMBSTONE, n3pg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_get (n4, PG_TOMBSTONE, n4pg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_get (n5, PG_TOMBSTONE, n5pg, f.p, &f.e), &f.e);

        test_err_t_wrap (pgr_release (f.p, n1, PG_TOMBSTONE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_TOMBSTONE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_TOMBSTONE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_TOMBSTONE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_TOMBSTONE, &f.e), &f.e);
      }

      TEST_CASE ("delete_chain inner node list starting at: %d", i)
      {
        struct page_tree_builder builder = in5in (f.p, &tx, 5, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_INNER_NODE, &f.e), &f.e);

        page_h *sut;

        switch (i)
          {
          case 0:
            {
              test_err_t_wrap (pgr_get (n1, PG_INNER_NODE, n1pg, f.p, &f.e), &f.e);
              sut = n1;
              break;
            }
          case 1:
            {
              test_err_t_wrap (pgr_get (n2, PG_INNER_NODE, n2pg, f.p, &f.e), &f.e);
              sut = n2;
              break;
            }
          case 2:
            {
              test_err_t_wrap (pgr_get (n3, PG_INNER_NODE, n3pg, f.p, &f.e), &f.e);
              sut = n3;
              break;
            }
          case 3:
            {
              test_err_t_wrap (pgr_get (n4, PG_INNER_NODE, n4pg, f.p, &f.e), &f.e);
              sut = n4;
              break;
            }
          case 4:
            {
              test_err_t_wrap (pgr_get (n5, PG_INNER_NODE, n5pg, f.p, &f.e), &f.e);
              sut = n5;
              break;
            }
          }

        test_err_t_wrap (pgr_dlgt_delete_chain (sut, &tx, f.p, &f.e), &f.e);

        test_assert_int_equal (n1->mode, PHM_NONE);
        test_assert_int_equal (n2->mode, PHM_NONE);
        test_assert_int_equal (n3->mode, PHM_NONE);
        test_assert_int_equal (n4->mode, PHM_NONE);
        test_assert_int_equal (n5->mode, PHM_NONE);

        test_err_t_wrap (pgr_get (n1, PG_TOMBSTONE, n1pg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_get (n2, PG_TOMBSTONE, n2pg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_get (n3, PG_TOMBSTONE, n3pg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_get (n4, PG_TOMBSTONE, n4pg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_get (n5, PG_TOMBSTONE, n5pg, f.p, &f.e), &f.e);

        test_err_t_wrap (pgr_release (f.p, n1, PG_TOMBSTONE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_TOMBSTONE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_TOMBSTONE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_TOMBSTONE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_TOMBSTONE, &f.e), &f.e);
      }
    }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

err_t
pgr_dlgt_delete_vpvl_chain (page_h *cur, struct txn *tx, struct pager *p, error *e)
{
  ASSERT (cur);
  ASSERT (tx);
  ASSERT (cur->mode != PHM_NONE);

  page_h next = page_h_create ();

  err_t_wrap (pgr_dlgt_get_ovnext (cur, &next, tx, p, e), e);

  err_t_wrap (pgr_delete_and_release (p, tx, cur, e), e);

  while (next.mode != PHM_NONE)
    {
      page_h next_next = page_h_create ();
      err_t_wrap (pgr_dlgt_get_next (&next, &next_next, tx, p, e), e);
      err_t_wrap (pgr_delete_and_release (p, tx, &next, e), e);
      page_h_xfer_ownership_ptr (&next, &next_next);
    }

  return SUCCESS;
}

/////////////////////////////////////////
/// FETCHING / CREATING NEIGHBORS

err_t
pgr_dlgt_get_next (
    page_h *cur,
    page_h *next,
    struct txn *tx,
    struct pager *p,
    error *e)
{
  ASSERT (cur->mode != PHM_NONE);

  pgno npg = dlgt_get_next (page_h_ro (cur));

  if (tx)
    {
      if (npg != PGNO_NULL)
        {
          if (next->mode == PHM_NONE)
            {
              err_t_wrap (pgr_get_writable (next, tx, page_get_type (page_h_ro (cur)), npg, p, e), e);
            }
          else
            {
              ASSERT (page_h_pgno (next) == npg);
            }
        }
    }
  else
    {
      if (npg != PGNO_NULL)
        {
          if (next->mode == PHM_NONE)
            {
              err_t_wrap (pgr_get (next, page_get_type (page_h_ro (cur)), npg, p, e), e);
            }
          else
            {
              ASSERT (page_h_pgno (next) == npg);
            }
        }
    }

  return SUCCESS;
}

TEST (TT_UNIT, pgr_dlgt_get_next)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  for (u32 i = 0; i < 5; ++i)
    {
      TEST_CASE ("data list starting at: %d", i)
      {
        struct page_tree_builder builder = in5dl (f.p, &tx, 5, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_DATA_LIST, &f.e), &f.e);

        page_h cur = page_h_create ();
        page_h dest = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];
        pgno epg = (pgno[]){ n2pg, n3pg, n4pg, n5pg, PGNO_NULL }[i];

        // X Mode
        test_err_t_wrap (pgr_get (&cur, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_dlgt_get_next (&cur, &dest, &tx, f.p, &f.e), &f.e);
        if (epg != PGNO_NULL)
          {
            test_assert_type_equal (epg, page_h_pgno (&dest), pgno, PRpgno);
            test_assert_int_equal (dest.mode, PHM_X);
            test_err_t_wrap (pgr_release (f.p, &dest, PG_DATA_LIST, &f.e), &f.e);
          }
        else
          {
            test_assert_int_equal (dest.mode, PHM_NONE);
          }

        // S Mode
        test_err_t_wrap (pgr_dlgt_get_next (&cur, &dest, NULL, f.p, &f.e), &f.e);
        if (epg != PGNO_NULL)
          {
            test_assert_type_equal (epg, page_h_pgno (&dest), pgno, PRpgno);
            test_assert_int_equal (dest.mode, PHM_S);
            test_err_t_wrap (pgr_release (f.p, &dest, PG_DATA_LIST, &f.e), &f.e);
          }
        else
          {
            test_assert_int_equal (dest.mode, PHM_NONE);
          }

        test_err_t_wrap (pgr_release (f.p, &cur, PG_DATA_LIST, &f.e), &f.e);
      }

      TEST_CASE ("inner node starting at: %d", i)
      {
        struct page_tree_builder builder = in5in (f.p, &tx, 5, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_INNER_NODE, &f.e), &f.e);

        page_h cur = page_h_create ();
        page_h dest = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];
        pgno epg = (pgno[]){ n2pg, n3pg, n4pg, n5pg, PGNO_NULL }[i];

        // X Mode
        test_err_t_wrap (pgr_get (&cur, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_dlgt_get_next (&cur, &dest, &tx, f.p, &f.e), &f.e);
        if (epg != PGNO_NULL)
          {
            test_assert_type_equal (epg, page_h_pgno (&dest), pgno, PRpgno);
            test_assert_int_equal (dest.mode, PHM_X);
            test_err_t_wrap (pgr_release (f.p, &dest, PG_INNER_NODE, &f.e), &f.e);
          }
        else
          {
            test_assert_int_equal (dest.mode, PHM_NONE);
          }

        // S Mode
        test_err_t_wrap (pgr_dlgt_get_next (&cur, &dest, NULL, f.p, &f.e), &f.e);
        if (epg != PGNO_NULL)
          {
            test_assert_type_equal (epg, page_h_pgno (&dest), pgno, PRpgno);
            test_assert_int_equal (dest.mode, PHM_S);
            test_err_t_wrap (pgr_release (f.p, &dest, PG_INNER_NODE, &f.e), &f.e);
          }
        else
          {
            test_assert_int_equal (dest.mode, PHM_NONE);
          }

        test_err_t_wrap (pgr_release (f.p, &cur, PG_INNER_NODE, &f.e), &f.e);
      }
    }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

err_t
pgr_dlgt_get_prev (page_h *cur, page_h *prev, struct txn *tx, struct pager *p, error *e)
{
  ASSERT (cur->mode != PHM_NONE);

  pgno ppg = dlgt_get_prev (page_h_ro (cur));

  if (tx)
    {
      if (ppg != PGNO_NULL)
        {
          if (prev->mode == PHM_NONE)
            {
              err_t_wrap (pgr_get_writable (prev, tx, page_get_type (page_h_ro (cur)), ppg, p, e), e);
            }
          else
            {
              ASSERT (page_h_pgno (prev) == ppg);
            }
        }
    }
  else
    {
      if (ppg != PGNO_NULL)
        {
          if (prev->mode == PHM_NONE)
            {
              err_t_wrap (pgr_get (prev, page_get_type (page_h_ro (cur)), ppg, p, e), e);
            }
          else
            {
              ASSERT (page_h_pgno (prev) == ppg);
            }
        }
    }

  return SUCCESS;
}

TEST (TT_UNIT, pgr_dlgt_get_prev)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  for (u32 i = 0; i < 5; ++i)
    {
      TEST_CASE ("data list starting at: %d", i)
      {
        struct page_tree_builder builder = in5dl (f.p, &tx, 5, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_DATA_LIST, &f.e), &f.e);

        page_h cur = page_h_create ();
        page_h dest = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];
        pgno epg = (pgno[]){ PGNO_NULL, n1pg, n2pg, n3pg, n4pg }[i];

        // X Mode
        test_err_t_wrap (pgr_get (&cur, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_dlgt_get_prev (&cur, &dest, &tx, f.p, &f.e), &f.e);
        if (epg != PGNO_NULL)
          {
            test_assert_type_equal (epg, page_h_pgno (&dest), pgno, PRpgno);
            test_assert_int_equal (dest.mode, PHM_X);
            test_err_t_wrap (pgr_release (f.p, &dest, PG_DATA_LIST, &f.e), &f.e);
          }
        else
          {
            test_assert_int_equal (dest.mode, PHM_NONE);
          }

        // S Mode
        test_err_t_wrap (pgr_dlgt_get_prev (&cur, &dest, NULL, f.p, &f.e), &f.e);
        if (epg != PGNO_NULL)
          {
            test_assert_type_equal (epg, page_h_pgno (&dest), pgno, PRpgno);
            test_assert_int_equal (dest.mode, PHM_S);
            test_err_t_wrap (pgr_release (f.p, &dest, PG_DATA_LIST, &f.e), &f.e);
          }
        else
          {
            test_assert_int_equal (dest.mode, PHM_NONE);
          }

        test_err_t_wrap (pgr_release (f.p, &cur, PG_DATA_LIST, &f.e), &f.e);
      }

      TEST_CASE ("inner node starting at: %d", i)
      {
        struct page_tree_builder builder = in5in (f.p, &tx, 5, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_INNER_NODE, &f.e), &f.e);

        page_h cur = page_h_create ();
        page_h dest = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];
        pgno epg = (pgno[]){ PGNO_NULL, n1pg, n2pg, n3pg, n4pg }[i];

        // X Mode
        test_err_t_wrap (pgr_get (&cur, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_dlgt_get_prev (&cur, &dest, &tx, f.p, &f.e), &f.e);
        if (epg != PGNO_NULL)
          {
            test_assert_type_equal (epg, page_h_pgno (&dest), pgno, PRpgno);
            test_assert_int_equal (dest.mode, PHM_X);
            test_err_t_wrap (pgr_release (f.p, &dest, PG_INNER_NODE, &f.e), &f.e);
          }
        else
          {
            test_assert_int_equal (dest.mode, PHM_NONE);
          }

        // S Mode
        test_err_t_wrap (pgr_dlgt_get_prev (&cur, &dest, NULL, f.p, &f.e), &f.e);
        if (epg != PGNO_NULL)
          {
            test_assert_type_equal (epg, page_h_pgno (&dest), pgno, PRpgno);
            test_assert_int_equal (dest.mode, PHM_S);
            test_err_t_wrap (pgr_release (f.p, &dest, PG_INNER_NODE, &f.e), &f.e);
          }
        else
          {
            test_assert_int_equal (dest.mode, PHM_NONE);
          }
        test_err_t_wrap (pgr_release (f.p, &cur, PG_INNER_NODE, &f.e), &f.e);
      }
    }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

err_t
pgr_dlgt_get_ovnext (page_h *cur, page_h *next, struct txn *tx, struct pager *p, error *e)
{
  ASSERT (cur->mode != PHM_NONE);

  pgno npg = dlgt_get_ovnext (page_h_ro (cur));

  if (tx)
    {
      if (npg != PGNO_NULL)
        {
          if (next->mode == PHM_NONE)
            {
              err_t_wrap (pgr_get_writable (next, tx, PG_VAR_TAIL, npg, p, e), e);
            }
          else
            {
              ASSERT (page_h_pgno (next) == npg);
            }
        }
    }
  else
    {
      if (npg != PGNO_NULL)
        {
          if (next->mode == PHM_NONE)
            {
              err_t_wrap (pgr_get (next, PG_VAR_TAIL, npg, p, e), e);
            }
          else
            {
              ASSERT (page_h_pgno (next) == npg);
            }
        }
    }

  return SUCCESS;
}

err_t
pgr_dlgt_get_neighbors (
    page_h *prev,
    page_h *cur,
    page_h *next,
    struct txn *tx,
    struct pager *p,
    error *e)
{
  ASSERT (cur->mode != PHM_NONE);

  err_t_wrap (pgr_dlgt_get_next (cur, next, tx, p, e), e);
  err_t_wrap (pgr_dlgt_get_prev (cur, prev, tx, p, e), e);

  return SUCCESS;
}

TEST (TT_UNIT, pgr_dlgt_get_neighbors)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  for (u32 i = 0; i < 5; ++i)
    {
      TEST_CASE ("new next data list starting at: %d", i)
      {
        struct page_tree_builder builder = in5dl (f.p, &tx, 5, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_DATA_LIST, &f.e), &f.e);

        page_h left = page_h_create ();
        page_h cur = page_h_create ();
        page_h right = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];
        pgno lpg = (pgno[]){ PGNO_NULL, n1pg, n2pg, n3pg, n4pg }[i];
        pgno rpg = (pgno[]){ n2pg, n3pg, n4pg, n5pg, PGNO_NULL }[i];

        // X Mode
        test_err_t_wrap (pgr_get (&cur, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_dlgt_get_neighbors (&left, &cur, &right, &tx, f.p, &f.e), &f.e);
        if (lpg != PGNO_NULL)
          {
            test_assert_type_equal (lpg, page_h_pgno (&left), pgno, PRpgno);
            test_assert_int_equal (left.mode, PHM_X);
            test_err_t_wrap (pgr_release (f.p, &left, PG_DATA_LIST, &f.e), &f.e);
          }
        else
          {
            test_assert_int_equal (left.mode, PHM_NONE);
          }
        if (rpg != PGNO_NULL)
          {
            test_assert_type_equal (rpg, page_h_pgno (&right), pgno, PRpgno);
            test_assert_int_equal (right.mode, PHM_X);
            test_err_t_wrap (pgr_release (f.p, &right, PG_DATA_LIST, &f.e), &f.e);
          }
        else
          {
            test_assert_int_equal (right.mode, PHM_NONE);
          }

        // S Mode
        test_err_t_wrap (pgr_dlgt_get_neighbors (&left, &cur, &right, NULL, f.p, &f.e), &f.e);
        if (lpg != PGNO_NULL)
          {
            test_assert_type_equal (lpg, page_h_pgno (&left), pgno, PRpgno);
            test_assert_int_equal (left.mode, PHM_S);
            test_err_t_wrap (pgr_release (f.p, &left, PG_DATA_LIST, &f.e), &f.e);
          }
        else
          {
            test_assert_int_equal (left.mode, PHM_NONE);
          }
        if (rpg != PGNO_NULL)
          {
            test_assert_type_equal (rpg, page_h_pgno (&right), pgno, PRpgno);
            test_assert_int_equal (right.mode, PHM_S);
            test_err_t_wrap (pgr_release (f.p, &right, PG_DATA_LIST, &f.e), &f.e);
          }
        else
          {
            test_assert_int_equal (right.mode, PHM_NONE);
          }

        test_err_t_wrap (pgr_release (f.p, &cur, PG_DATA_LIST, &f.e), &f.e);
      }

      TEST_CASE ("data list starting at: %d", i)
      {
        struct page_tree_builder builder = in5in (f.p, &tx, 5, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_INNER_NODE, &f.e), &f.e);

        page_h left = page_h_create ();
        page_h cur = page_h_create ();
        page_h right = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];
        pgno lpg = (pgno[]){ PGNO_NULL, n1pg, n2pg, n3pg, n4pg }[i];
        pgno rpg = (pgno[]){ n2pg, n3pg, n4pg, n5pg, PGNO_NULL }[i];

        // X Mode
        test_err_t_wrap (pgr_get (&cur, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_dlgt_get_neighbors (&left, &cur, &right, &tx, f.p, &f.e), &f.e);
        if (lpg != PGNO_NULL)
          {
            test_assert_type_equal (lpg, page_h_pgno (&left), pgno, PRpgno);
            test_assert_int_equal (left.mode, PHM_X);
            test_err_t_wrap (pgr_release (f.p, &left, PG_INNER_NODE, &f.e), &f.e);
          }
        else
          {
            test_assert_int_equal (left.mode, PHM_NONE);
          }
        if (rpg != PGNO_NULL)
          {
            test_assert_type_equal (rpg, page_h_pgno (&right), pgno, PRpgno);
            test_assert_int_equal (right.mode, PHM_X);
            test_err_t_wrap (pgr_release (f.p, &right, PG_INNER_NODE, &f.e), &f.e);
          }
        else
          {
            test_assert_int_equal (right.mode, PHM_NONE);
          }

        // S Mode
        test_err_t_wrap (pgr_dlgt_get_neighbors (&left, &cur, &right, NULL, f.p, &f.e), &f.e);
        if (lpg != PGNO_NULL)
          {
            test_assert_type_equal (lpg, page_h_pgno (&left), pgno, PRpgno);
            test_assert_int_equal (left.mode, PHM_S);
            test_err_t_wrap (pgr_release (f.p, &left, PG_INNER_NODE, &f.e), &f.e);
          }
        else
          {
            test_assert_int_equal (left.mode, PHM_NONE);
          }
        if (rpg != PGNO_NULL)
          {
            test_assert_type_equal (rpg, page_h_pgno (&right), pgno, PRpgno);
            test_assert_int_equal (right.mode, PHM_S);
            test_err_t_wrap (pgr_release (f.p, &right, PG_INNER_NODE, &f.e), &f.e);
          }
        else
          {
            test_assert_int_equal (right.mode, PHM_NONE);
          }

        test_err_t_wrap (pgr_release (f.p, &cur, PG_INNER_NODE, &f.e), &f.e);
      }
    }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

err_t
pgr_dlgt_new_next (page_h *cur, page_h *dest, page_h *c_next, struct txn *tx, struct pager *p, error *e)
{
  ASSERT (cur->mode != PHM_NONE);
  ASSERT (dest->mode == PHM_NONE);
  ASSERT (dlgt_valid_neighbors (page_h_ro_or_null (cur), page_h_ro_or_null (c_next)));

  err_t_wrap (pgr_maybe_make_writable (p, tx, cur, e), e);
  err_t_wrap (pgr_maybe_make_writable (p, tx, c_next, e), e);

  err_t_wrap (pgr_new (dest, p, tx, page_get_type (page_h_ro (cur)), e), e);
  dlgt_link (page_h_w (cur), page_h_w (dest));
  dlgt_link (page_h_w (dest), page_h_w_or_null (c_next));

  return SUCCESS;
}

TEST (TT_UNIT, pgr_dlgt_new_next)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  for (u32 i = 0; i < 5; ++i)
    {
      TEST_CASE ("data list starting at: %d", i)
      {
        struct page_tree_builder builder = in5dl (f.p, &tx, 5, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_DATA_LIST, &f.e), &f.e);

        page_h cur = page_h_create ();
        page_h new_next = page_h_create ();
        page_h next = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];
        pgno npg = (pgno[]){ n2pg, n3pg, n4pg, n5pg, PGNO_NULL }[i];

        // X Mode
        test_err_t_wrap (pgr_get (&cur, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);
        if (npg != PGNO_NULL)
          {
            test_err_t_wrap (pgr_get (&next, PG_DATA_LIST, npg, f.p, &f.e), &f.e);
          }
        test_err_t_wrap (pgr_dlgt_new_next (&cur, &new_next, &next, &tx, f.p, &f.e), &f.e);
        dl_make_valid (page_h_w (&new_next));

        test_assert (dlgt_valid_neighbors (page_h_ro (&cur), page_h_ro (&new_next)));
        test_assert (dlgt_valid_neighbors (page_h_ro (&new_next), page_h_ro_or_null (&next)));

        test_err_t_wrap (pgr_release (f.p, &cur, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, &new_next, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release_if_exists (f.p, &next, PG_DATA_LIST, &f.e), &f.e);
      }

      TEST_CASE ("inner node starting at: %d", i)
      {
        struct page_tree_builder builder = in5in (f.p, &tx, 5, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_INNER_NODE, &f.e), &f.e);

        page_h cur = page_h_create ();
        page_h new_next = page_h_create ();
        page_h next = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];
        pgno npg = (pgno[]){ n2pg, n3pg, n4pg, n5pg, PGNO_NULL }[i];

        // X Mode
        test_err_t_wrap (pgr_get (&cur, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);
        if (npg != PGNO_NULL)
          {
            test_err_t_wrap (pgr_get (&next, PG_INNER_NODE, npg, f.p, &f.e), &f.e);
          }
        test_err_t_wrap (pgr_dlgt_new_next (&cur, &new_next, &next, &tx, f.p, &f.e), &f.e);
        in_make_valid (page_h_w (&new_next));

        test_assert (dlgt_valid_neighbors (page_h_ro (&cur), page_h_ro (&new_next)));
        test_assert (dlgt_valid_neighbors (page_h_ro (&new_next), page_h_ro_or_null (&next)));

        test_err_t_wrap (pgr_release (f.p, &cur, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, &new_next, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release_if_exists (f.p, &next, PG_INNER_NODE, &f.e), &f.e);
      }
    }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

err_t
pgr_dlgt_new_prev (page_h *c_prev, page_h *dest, page_h *cur, struct txn *tx, struct pager *p, error *e)
{
  ASSERT (cur->mode != PHM_NONE);
  ASSERT (dest->mode == PHM_NONE);
  ASSERT (dlgt_valid_neighbors (page_h_ro_or_null (c_prev), page_h_ro (cur)));

  err_t_wrap (pgr_maybe_make_writable (p, tx, cur, e), e);
  err_t_wrap (pgr_maybe_make_writable (p, tx, c_prev, e), e);

  err_t_wrap (pgr_new (dest, p, tx, page_get_type (page_h_ro (cur)), e), e);
  dlgt_link (page_h_w_or_null (c_prev), page_h_w (dest));
  dlgt_link (page_h_w (dest), page_h_w (cur));

  return SUCCESS;
}

TEST (TT_UNIT, pgr_dlgt_new_prev)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  for (u32 i = 0; i < 5; ++i)
    {
      TEST_CASE ("data list starting at: %d", i)
      {
        struct page_tree_builder builder = in5dl (f.p, &tx, 5, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_DATA_LIST, &f.e), &f.e);

        page_h prev = page_h_create ();
        page_h new_prev = page_h_create ();
        page_h cur = page_h_create ();

        pgno ppg = (pgno[]){ PGNO_NULL, n1pg, n2pg, n3pg, n4pg }[i];
        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];

        // X Mode
        test_err_t_wrap (pgr_get (&cur, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);
        if (ppg != PGNO_NULL)
          {
            test_err_t_wrap (pgr_get (&prev, PG_DATA_LIST, ppg, f.p, &f.e), &f.e);
          }
        test_err_t_wrap (pgr_dlgt_new_prev (&prev, &new_prev, &cur, &tx, f.p, &f.e), &f.e);
        dl_make_valid (page_h_w (&new_prev));

        test_assert (dlgt_valid_neighbors (page_h_ro_or_null (&prev), page_h_ro (&new_prev)));
        test_assert (dlgt_valid_neighbors (page_h_ro (&new_prev), page_h_ro (&cur)));

        test_err_t_wrap (pgr_release (f.p, &cur, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, &new_prev, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release_if_exists (f.p, &prev, PG_DATA_LIST, &f.e), &f.e);
      }

      TEST_CASE ("inner node starting at: %d", i)
      {
        struct page_tree_builder builder = in5in (f.p, &tx, 5, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_INNER_NODE, &f.e), &f.e);

        page_h prev = page_h_create ();
        page_h new_prev = page_h_create ();
        page_h cur = page_h_create ();

        pgno ppg = (pgno[]){ PGNO_NULL, n1pg, n2pg, n3pg, n4pg }[i];
        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];

        // X Mode
        test_err_t_wrap (pgr_get (&cur, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);
        if (ppg != PGNO_NULL)
          {
            test_err_t_wrap (pgr_get (&prev, PG_INNER_NODE, ppg, f.p, &f.e), &f.e);
          }
        test_err_t_wrap (pgr_dlgt_new_prev (&prev, &new_prev, &cur, &tx, f.p, &f.e), &f.e);
        in_make_valid (page_h_w (&new_prev));

        test_assert (dlgt_valid_neighbors (page_h_ro_or_null (&prev), page_h_ro (&new_prev)));
        test_assert (dlgt_valid_neighbors (page_h_ro (&new_prev), page_h_ro (&cur)));

        test_err_t_wrap (pgr_release (f.p, &cur, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, &new_prev, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release_if_exists (f.p, &prev, PG_INNER_NODE, &f.e), &f.e);
      }
    }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

err_t
pgr_dlgt_new_next_no_link (page_h *cur, page_h *next, struct txn *tx, struct pager *p, error *e)
{
  ASSERT (cur->mode != PHM_NONE);

  err_t_wrap (pgr_maybe_make_writable (p, tx, cur, e), e);

  err_t_wrap (pgr_new (next, p, tx, page_get_type (page_h_ro (cur)), e), e);
  dlgt_link (page_h_w (cur), page_h_w (next));

  return SUCCESS;
}

TEST (TT_UNIT, pgr_dlgt_new_next_no_link)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  for (u32 i = 0; i < 5; ++i)
    {
      TEST_CASE ("data list starting at: %d", i)
      {
        struct page_tree_builder builder = in5dl (f.p, &tx, 5, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_DATA_LIST, &f.e), &f.e);

        page_h cur = page_h_create ();
        page_h next = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];

        // X Mode
        test_err_t_wrap (pgr_get (&cur, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_dlgt_new_next_no_link (&cur, &next, &tx, f.p, &f.e), &f.e);
        dl_make_valid (page_h_w (&next));

        test_assert (dlgt_valid_neighbors (page_h_ro (&cur), page_h_ro (&next)));

        test_err_t_wrap (pgr_release (f.p, &cur, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, &next, PG_DATA_LIST, &f.e), &f.e);
      }

      TEST_CASE ("inner node starting at: %d", i)
      {
        struct page_tree_builder builder = in5in (f.p, &tx, 5, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_INNER_NODE, &f.e), &f.e);

        page_h cur = page_h_create ();
        page_h next = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];

        // X Mode
        test_err_t_wrap (pgr_get (&cur, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_dlgt_new_next_no_link (&cur, &next, &tx, f.p, &f.e), &f.e);
        in_make_valid (page_h_w (&next));

        test_assert (dlgt_valid_neighbors (page_h_ro (&cur), page_h_ro (&next)));

        test_err_t_wrap (pgr_release (f.p, &cur, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, &next, PG_INNER_NODE, &f.e), &f.e);
      }
    }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

err_t
pgr_dlgt_new_prev_no_link (page_h *cur, page_h *prev, struct txn *tx, struct pager *p, error *e)
{
  ASSERT (cur->mode != PHM_NONE);

  err_t_wrap (pgr_maybe_make_writable (p, tx, cur, e), e);

  err_t_wrap (pgr_new (prev, p, tx, page_get_type (page_h_ro (cur)), e), e);
  dlgt_link (page_h_w (prev), page_h_w (cur));

  return SUCCESS;
}

TEST (TT_UNIT, pgr_dlgt_new_prev_no_link)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  for (u32 i = 0; i < 5; ++i)
    {
      TEST_CASE ("data list starting at: %d", i)
      {
        struct page_tree_builder builder = in5dl (f.p, &tx, 5, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_DATA_LIST, &f.e), &f.e);

        page_h prev = page_h_create ();
        page_h cur = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];

        // X Mode
        test_err_t_wrap (pgr_get (&cur, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_dlgt_new_prev_no_link (&cur, &prev, &tx, f.p, &f.e), &f.e);
        dl_make_valid (page_h_w (&prev));

        test_assert (dlgt_valid_neighbors (page_h_ro (&prev), page_h_ro (&cur)));

        test_err_t_wrap (pgr_release (f.p, &cur, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, &prev, PG_DATA_LIST, &f.e), &f.e);
      }

      TEST_CASE ("inner node starting at: %d", i)
      {
        struct page_tree_builder builder = in5in (f.p, &tx, 5, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_INNER_NODE, &f.e), &f.e);

        page_h prev = page_h_create ();
        page_h cur = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];

        // X Mode
        test_err_t_wrap (pgr_get (&cur, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_dlgt_new_prev_no_link (&cur, &prev, &tx, f.p, &f.e), &f.e);
        in_make_valid (page_h_w (&prev));

        test_assert (dlgt_valid_neighbors (page_h_ro (&prev), page_h_ro (&cur)));

        test_err_t_wrap (pgr_release (f.p, &cur, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, &prev, PG_INNER_NODE, &f.e), &f.e);
      }
    }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

err_t
pgr_dlgt_new_ovnext_no_link (page_h *cur, page_h *next, struct txn *tx, struct pager *p, error *e)
{
  ASSERT (cur->mode != PHM_NONE);

  err_t_wrap (pgr_maybe_make_writable (p, tx, cur, e), e);

  err_t_wrap (pgr_new (next, p, tx, PG_VAR_TAIL, e), e);
  dlgtovlink (page_h_w (cur), page_h_w (next));

  return SUCCESS;
}

err_t
pgr_dlgt_advance_next (page_h *cur, struct txn *tx, struct pager *p, error *e)
{
  ASSERT (cur->mode != PHM_NONE);

  page_h next = page_h_create ();
  pgno npg = dlgt_get_next (page_h_ro (cur));
  enum page_type type = page_get_type (page_h_ro (cur));

  err_t_wrap (pgr_release (p, cur, type, e), e);
  if (tx)
    {
      if (npg != PGNO_NULL)
        {
          err_t_wrap (pgr_get_writable (&next, tx, type, npg, p, e), e);
        }
    }
  else
    {
      if (npg != PGNO_NULL)
        {
          err_t_wrap (pgr_get (&next, type, npg, p, e), e);
        }
    }

  *cur = next;

  return SUCCESS;
}

TEST (TT_UNIT, pgr_dlgt_advance_next)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  for (u32 i = 0; i < 5; ++i)
    {
      TEST_CASE ("data list starting at: %d", i)
      {
        struct page_tree_builder builder = in5dl (f.p, &tx, 5, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_DATA_LIST, &f.e), &f.e);

        page_h cur = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];
        pgno npg = (pgno[]){ n2pg, n3pg, n4pg, n5pg, PGNO_NULL }[i];

        // X Mode
        test_err_t_wrap (pgr_get (&cur, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_dlgt_advance_next (&cur, &tx, f.p, &f.e), &f.e);
        if (npg == PGNO_NULL)
          {
            test_assert_int_equal (cur.mode, PHM_NONE);
          }
        else
          {
            test_assert_int_equal (cur.mode, PHM_X);
            test_assert_type_equal (page_h_pgno (&cur), npg, pgno, PRpgno);
            test_err_t_wrap (pgr_release (f.p, &cur, PG_DATA_LIST, &f.e), &f.e);
          }

        // S Mode
        test_err_t_wrap (pgr_get (&cur, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_dlgt_advance_next (&cur, NULL, f.p, &f.e), &f.e);
        if (npg == PGNO_NULL)
          {
            test_assert_int_equal (cur.mode, PHM_NONE);
          }
        else
          {
            test_assert_int_equal (cur.mode, PHM_S);
            test_assert_type_equal (page_h_pgno (&cur), npg, pgno, PRpgno);
            test_err_t_wrap (pgr_release (f.p, &cur, PG_DATA_LIST, &f.e), &f.e);
          }
      }

      TEST_CASE ("inner node starting at: %d", i)
      {
        struct page_tree_builder builder = in5in (f.p, &tx, 5, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_INNER_NODE, &f.e), &f.e);

        page_h cur = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];
        pgno npg = (pgno[]){ n2pg, n3pg, n4pg, n5pg, PGNO_NULL }[i];

        // X Mode
        test_err_t_wrap (pgr_get (&cur, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_dlgt_advance_next (&cur, &tx, f.p, &f.e), &f.e);
        if (npg == PGNO_NULL)
          {
            test_assert_int_equal (cur.mode, PHM_NONE);
          }
        else
          {
            test_assert_int_equal (cur.mode, PHM_X);
            test_assert_type_equal (page_h_pgno (&cur), npg, pgno, PRpgno);
            test_err_t_wrap (pgr_release (f.p, &cur, PG_INNER_NODE, &f.e), &f.e);
          }

        // S Mode
        test_err_t_wrap (pgr_get (&cur, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_dlgt_advance_next (&cur, NULL, f.p, &f.e), &f.e);
        if (npg == PGNO_NULL)
          {
            test_assert_int_equal (cur.mode, PHM_NONE);
          }
        else
          {
            test_assert_int_equal (cur.mode, PHM_S);
            test_assert_type_equal (page_h_pgno (&cur), npg, pgno, PRpgno);
            test_err_t_wrap (pgr_release (f.p, &cur, PG_INNER_NODE, &f.e), &f.e);
          }
      }
    }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

err_t
pgr_dlgt_advance_prev (page_h *cur, struct txn *tx, struct pager *p, error *e)
{
  ASSERT (cur->mode != PHM_NONE);

  page_h prev = page_h_create ();
  pgno ppg = dlgt_get_prev (page_h_ro (cur));
  enum page_type type = page_get_type (page_h_ro (cur));

  err_t_wrap (pgr_release (p, cur, type, e), e);
  if (tx)
    {
      if (ppg != PGNO_NULL)
        {
          err_t_wrap (pgr_get_writable (&prev, tx, type, ppg, p, e), e);
        }
    }
  else
    {
      if (ppg != PGNO_NULL)
        {
          err_t_wrap (pgr_get (&prev, type, ppg, p, e), e);
        }
    }

  *cur = prev;

  return SUCCESS;
}

TEST (TT_UNIT, pgr_dlgt_advance_prev)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  for (u32 i = 0; i < 5; ++i)
    {
      TEST_CASE ("data list starting at: %d", i)
      {
        struct page_tree_builder builder = in5dl (f.p, &tx, 5, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_DATA_LIST, &f.e), &f.e);

        page_h cur = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];
        pgno npg = (pgno[]){ PGNO_NULL, n1pg, n2pg, n3pg, n4pg }[i];

        // X Mode
        test_err_t_wrap (pgr_get (&cur, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_dlgt_advance_prev (&cur, &tx, f.p, &f.e), &f.e);
        if (npg == PGNO_NULL)
          {
            test_assert_int_equal (cur.mode, PHM_NONE);
          }
        else
          {
            test_assert_int_equal (cur.mode, PHM_X);
            test_assert_type_equal (page_h_pgno (&cur), npg, pgno, PRpgno);
            test_err_t_wrap (pgr_release (f.p, &cur, PG_DATA_LIST, &f.e), &f.e);
          }

        // S Mode
        test_err_t_wrap (pgr_get (&cur, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_dlgt_advance_prev (&cur, NULL, f.p, &f.e), &f.e);
        if (npg == PGNO_NULL)
          {
            test_assert_int_equal (cur.mode, PHM_NONE);
          }
        else
          {
            test_assert_int_equal (cur.mode, PHM_S);
            test_assert_type_equal (page_h_pgno (&cur), npg, pgno, PRpgno);
            test_err_t_wrap (pgr_release (f.p, &cur, PG_DATA_LIST, &f.e), &f.e);
          }
      }

      TEST_CASE ("inner node starting at: %d", i)
      {
        struct page_tree_builder builder = in5in (f.p, &tx, 5, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_INNER_NODE, &f.e), &f.e);

        page_h cur = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];
        pgno npg = (pgno[]){ PGNO_NULL, n1pg, n2pg, n3pg, n4pg }[i];

        // X Mode
        test_err_t_wrap (pgr_get (&cur, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_dlgt_advance_prev (&cur, &tx, f.p, &f.e), &f.e);
        if (npg == PGNO_NULL)
          {
            test_assert_int_equal (cur.mode, PHM_NONE);
          }
        else
          {
            test_assert_int_equal (cur.mode, PHM_X);
            test_assert_type_equal (page_h_pgno (&cur), npg, pgno, PRpgno);
            test_err_t_wrap (pgr_release (f.p, &cur, PG_INNER_NODE, &f.e), &f.e);
          }

        // S Mode
        test_err_t_wrap (pgr_get (&cur, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);
        test_err_t_wrap (pgr_dlgt_advance_prev (&cur, NULL, f.p, &f.e), &f.e);
        if (npg == PGNO_NULL)
          {
            test_assert_int_equal (cur.mode, PHM_NONE);
          }
        else
          {
            test_assert_int_equal (cur.mode, PHM_S);
            test_assert_type_equal (page_h_pgno (&cur), npg, pgno, PRpgno);
            test_err_t_wrap (pgr_release (f.p, &cur, PG_INNER_NODE, &f.e), &f.e);
          }
      }
    }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

err_t
pgr_dlgt_advance_new_next (page_h *cur, page_h *c_next, struct txn *tx, struct pager *p, error *e)
{
  ASSERT (cur->mode != PHM_NONE);
  ASSERT (dlgt_valid_neighbors (page_h_ro (cur), page_h_ro_or_null (c_next)));

  err_t_wrap (pgr_maybe_make_writable (p, tx, cur, e), e);
  err_t_wrap (pgr_maybe_make_writable (p, tx, c_next, e), e);

  page_h next = page_h_create ();
  err_t_wrap (pgr_new (&next, p, tx, page_get_type (page_h_ro (cur)), e), e);
  dlgt_link (page_h_w (cur), page_h_w (&next));
  dlgt_link (page_h_w (&next), page_h_w_or_null (c_next));
  err_t_wrap (pgr_release (p, cur, page_get_type (page_h_ro (cur)), e), e);

  *cur = page_h_xfer_ownership (&next);

  return SUCCESS;
}

TEST (TT_UNIT, pgr_dlgt_advance_new_next)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  for (u32 i = 0; i < 5; ++i)
    {
      TEST_CASE ("data list starting at: %d", i)
      {
        struct page_tree_builder builder = in5dl (f.p, &tx, 5, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_DATA_LIST, &f.e), &f.e);

        page_h cprev = page_h_create ();
        page_h cur = page_h_create ();
        page_h c_next = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];
        pgno npg = (pgno[]){ n2pg, n3pg, n4pg, n5pg, PGNO_NULL }[i];

        test_err_t_wrap (pgr_get (&cur, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);
        if (npg != PGNO_NULL)
          {
            test_err_t_wrap (pgr_get (&c_next, PG_DATA_LIST, npg, f.p, &f.e), &f.e);
          }

        test_err_t_wrap (pgr_dlgt_advance_new_next (&cur, &c_next, &tx, f.p, &f.e), &f.e);
        dl_make_valid (page_h_w (&cur));

        // re fetch cur
        test_err_t_wrap (pgr_get (&cprev, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);

        test_assert (dlgt_valid_neighbors (page_h_ro (&cprev), page_h_ro (&cur)));
        test_assert (dlgt_valid_neighbors (page_h_ro (&cur), page_h_ro_or_null (&c_next)));
        test_assert (page_h_pgno (&cprev) != page_h_pgno (&cur));

        test_err_t_wrap (pgr_release (f.p, &cprev, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, &cur, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release_if_exists (f.p, &c_next, PG_DATA_LIST, &f.e), &f.e);
      }

      TEST_CASE ("inner node starting at: %d", i)
      {
        struct page_tree_builder builder = in5in (f.p, &tx, 5, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_INNER_NODE, &f.e), &f.e);

        page_h cprev = page_h_create ();
        page_h cur = page_h_create ();
        page_h c_next = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];
        pgno npg = (pgno[]){ n2pg, n3pg, n4pg, n5pg, PGNO_NULL }[i];

        test_err_t_wrap (pgr_get (&cur, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);
        if (npg != PGNO_NULL)
          {
            test_err_t_wrap (pgr_get (&c_next, PG_INNER_NODE, npg, f.p, &f.e), &f.e);
          }

        test_err_t_wrap (pgr_dlgt_advance_new_next (&cur, &c_next, &tx, f.p, &f.e), &f.e);
        in_make_valid (page_h_w (&cur));

        // re fetch cur
        test_err_t_wrap (pgr_get (&cprev, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);

        test_assert (dlgt_valid_neighbors (page_h_ro (&cprev), page_h_ro (&cur)));
        test_assert (dlgt_valid_neighbors (page_h_ro (&cur), page_h_ro_or_null (&c_next)));
        test_assert (page_h_pgno (&cprev) != page_h_pgno (&cur));

        test_err_t_wrap (pgr_release (f.p, &cprev, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, &cur, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release_if_exists (f.p, &c_next, PG_INNER_NODE, &f.e), &f.e);
      }
    }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

err_t
pgr_dlgt_advance_new_prev (page_h *c_prev, page_h *cur, struct txn *tx, struct pager *p, error *e)
{
  ASSERT (cur->mode != PHM_NONE);
  ASSERT (dlgt_valid_neighbors (page_h_ro_or_null (c_prev), page_h_ro (cur)));

  err_t_wrap (pgr_maybe_make_writable (p, tx, c_prev, e), e);
  err_t_wrap (pgr_maybe_make_writable (p, tx, cur, e), e);

  page_h prev = page_h_create ();
  err_t_wrap (pgr_new (&prev, p, tx, page_get_type (page_h_ro (cur)), e), e);

  dlgt_link (page_h_w_or_null (c_prev), page_h_w (&prev));
  dlgt_link (page_h_w (&prev), page_h_w (cur));

  err_t_wrap (pgr_release (p, cur, page_get_type (page_h_ro (cur)), e), e);

  *cur = page_h_xfer_ownership (&prev);

  return SUCCESS;
}

TEST (TT_UNIT, pgr_dlgt_advance_new_prev)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  for (u32 i = 0; i < 5; ++i)
    {
      TEST_CASE ("data list starting at: %d", i)
      {
        struct page_tree_builder builder = in5dl (f.p, &tx, 5, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_DATA_LIST, &f.e), &f.e);

        page_h c_prev = page_h_create ();
        page_h cur = page_h_create ();
        page_h cnext = page_h_create ();

        pgno ppg = (pgno[]){ PGNO_NULL, n1pg, n2pg, n3pg, n4pg }[i];
        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];

        if (ppg != PGNO_NULL)
          {
            test_err_t_wrap (pgr_get (&c_prev, PG_DATA_LIST, ppg, f.p, &f.e), &f.e);
          }
        test_err_t_wrap (pgr_get (&cur, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);

        test_err_t_wrap (pgr_dlgt_advance_new_prev (&c_prev, &cur, &tx, f.p, &f.e), &f.e);
        dl_make_valid (page_h_w (&cur));

        // re fetch cur
        test_err_t_wrap (pgr_get (&cnext, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);

        test_assert (dlgt_valid_neighbors (page_h_ro_or_null (&c_prev), page_h_ro (&cur)));
        test_assert (dlgt_valid_neighbors (page_h_ro (&cur), page_h_ro_or_null (&cnext)));
        test_assert (page_h_pgno (&cnext) != page_h_pgno (&cur));

        test_err_t_wrap (pgr_release_if_exists (f.p, &c_prev, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, &cur, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, &cnext, PG_DATA_LIST, &f.e), &f.e);
      }

      TEST_CASE ("data list starting at: %d", i)
      {
        struct page_tree_builder builder = in5in (f.p, &tx, 5, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_INNER_NODE, &f.e), &f.e);

        page_h c_prev = page_h_create ();
        page_h cur = page_h_create ();
        page_h cnext = page_h_create ();

        pgno ppg = (pgno[]){ PGNO_NULL, n1pg, n2pg, n3pg, n4pg }[i];
        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];

        if (ppg != PGNO_NULL)
          {
            test_err_t_wrap (pgr_get (&c_prev, PG_INNER_NODE, ppg, f.p, &f.e), &f.e);
          }
        test_err_t_wrap (pgr_get (&cur, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);

        test_err_t_wrap (pgr_dlgt_advance_new_prev (&c_prev, &cur, &tx, f.p, &f.e), &f.e);
        in_make_valid (page_h_w (&cur));

        // re fetch cur
        test_err_t_wrap (pgr_get (&cnext, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);

        test_assert (dlgt_valid_neighbors (page_h_ro_or_null (&c_prev), page_h_ro (&cur)));
        test_assert (dlgt_valid_neighbors (page_h_ro (&cur), page_h_ro_or_null (&cnext)));
        test_assert (page_h_pgno (&cnext) != page_h_pgno (&cur));

        test_err_t_wrap (pgr_release_if_exists (f.p, &c_prev, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, &cur, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, &cnext, PG_INNER_NODE, &f.e), &f.e);
      }
    }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

err_t
pgr_dlgt_advance_new_next_no_link (page_h *cur, struct txn *tx, struct pager *p, error *e)
{
  ASSERT (cur->mode != PHM_NONE);

  err_t_wrap (pgr_maybe_make_writable (p, tx, cur, e), e);

  page_h next = page_h_create ();
  err_t_wrap (pgr_new (&next, p, tx, page_get_type (page_h_ro (cur)), e), e);
  dlgt_link (page_h_w (cur), page_h_w (&next));
  err_t_wrap (pgr_release (p, cur, page_get_type (page_h_ro (cur)), e), e);

  *cur = next;

  return SUCCESS;
}

TEST (TT_UNIT, pgr_dlgt_advance_new_next_no_link)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  for (u32 i = 0; i < 5; ++i)
    {
      TEST_CASE ("data list starting at: %d", i)
      {
        struct page_tree_builder builder = in5dl (f.p, &tx, 5, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_DATA_LIST, &f.e), &f.e);

        page_h cprev = page_h_create ();
        page_h cur = page_h_create ();
        page_h c_next = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];

        test_err_t_wrap (pgr_get (&cur, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);

        test_err_t_wrap (pgr_dlgt_advance_new_next_no_link (&cur, &tx, f.p, &f.e), &f.e);
        dl_make_valid (page_h_w (&cur));

        // re fetch cur
        test_err_t_wrap (pgr_get (&cprev, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);

        test_assert (dlgt_valid_neighbors (page_h_ro (&cprev), page_h_ro (&cur)));
        test_assert (dlgt_valid_neighbors (page_h_ro (&cur), NULL));
        test_assert (page_h_pgno (&cprev) != page_h_pgno (&cur));

        test_err_t_wrap (pgr_release (f.p, &cprev, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, &cur, PG_DATA_LIST, &f.e), &f.e);
      }

      TEST_CASE ("inner node starting at: %d", i)
      {
        struct page_tree_builder builder = in5in (f.p, &tx, 5, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_INNER_NODE, &f.e), &f.e);

        page_h cprev = page_h_create ();
        page_h cur = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];

        test_err_t_wrap (pgr_get (&cur, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);

        test_err_t_wrap (pgr_dlgt_advance_new_next_no_link (&cur, &tx, f.p, &f.e), &f.e);
        in_make_valid (page_h_w (&cur));

        // re fetch cur
        test_err_t_wrap (pgr_get (&cprev, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);

        test_assert (dlgt_valid_neighbors (page_h_ro (&cprev), page_h_ro (&cur)));
        test_assert (dlgt_valid_neighbors (page_h_ro (&cur), NULL));
        test_assert (page_h_pgno (&cprev) != page_h_pgno (&cur));

        test_err_t_wrap (pgr_release (f.p, &cprev, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, &cur, PG_INNER_NODE, &f.e), &f.e);
      }
    }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

err_t
pgr_dlgt_advance_new_prev_no_link (page_h *cur, struct txn *tx, struct pager *p, error *e)
{
  ASSERT (cur->mode != PHM_NONE);

  err_t_wrap (pgr_maybe_make_writable (p, tx, cur, e), e);

  page_h prev = page_h_create ();
  err_t_wrap (pgr_new (&prev, p, tx, page_get_type (page_h_ro (cur)), e), e);
  dlgt_link (page_h_w (&prev), page_h_w (cur));
  err_t_wrap (pgr_release (p, cur, page_get_type (page_h_ro (cur)), e), e);

  *cur = prev;

  return SUCCESS;
}

TEST (TT_UNIT, pgr_dlgt_advance_new_prev_no_link)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  for (u32 i = 0; i < 5; ++i)
    {
      TEST_CASE ("data list starting at: %d", i)
      {
        struct page_tree_builder builder = in5dl (f.p, &tx, 5, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_DATA_LIST, &f.e), &f.e);

        page_h cur = page_h_create ();
        page_h cnext = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];

        test_err_t_wrap (pgr_get (&cur, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);

        test_err_t_wrap (pgr_dlgt_advance_new_prev_no_link (&cur, &tx, f.p, &f.e), &f.e);
        dl_make_valid (page_h_w (&cur));

        // re fetch cur
        test_err_t_wrap (pgr_get (&cnext, PG_DATA_LIST, cpg, f.p, &f.e), &f.e);

        test_assert (dlgt_valid_neighbors (NULL, page_h_ro (&cur)));
        test_assert (dlgt_valid_neighbors (page_h_ro (&cur), page_h_ro_or_null (&cnext)));
        test_assert (page_h_pgno (&cnext) != page_h_pgno (&cur));

        test_err_t_wrap (pgr_release (f.p, &cur, PG_DATA_LIST, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, &cnext, PG_DATA_LIST, &f.e), &f.e);
      }

      TEST_CASE ("data list starting at: %d", i)
      {
        struct page_tree_builder builder = in5in (f.p, &tx, 5, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS, IN_MAX_KEYS);
        test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

        page_h *root = &builder.root.out;
        page_h *n1 = &builder.root.inner.children[0].out;
        page_h *n2 = &builder.root.inner.children[1].out;
        page_h *n3 = &builder.root.inner.children[2].out;
        page_h *n4 = &builder.root.inner.children[3].out;
        page_h *n5 = &builder.root.inner.children[4].out;

        pgno n1pg = page_h_pgno (n1);
        pgno n2pg = page_h_pgno (n2);
        pgno n3pg = page_h_pgno (n3);
        pgno n4pg = page_h_pgno (n4);
        pgno n5pg = page_h_pgno (n5);

        test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n1, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n2, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n3, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n4, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, n5, PG_INNER_NODE, &f.e), &f.e);

        page_h cur = page_h_create ();
        page_h cnext = page_h_create ();

        pgno cpg = (pgno[]){ n1pg, n2pg, n3pg, n4pg, n5pg }[i];

        test_err_t_wrap (pgr_get (&cur, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);

        test_err_t_wrap (pgr_dlgt_advance_new_prev_no_link (&cur, &tx, f.p, &f.e), &f.e);
        in_make_valid (page_h_w (&cur));

        // re fetch cur
        test_err_t_wrap (pgr_get (&cnext, PG_INNER_NODE, cpg, f.p, &f.e), &f.e);

        test_assert (dlgt_valid_neighbors (NULL, page_h_ro (&cur)));
        test_assert (dlgt_valid_neighbors (page_h_ro (&cur), page_h_ro_or_null (&cnext)));
        test_assert (page_h_pgno (&cnext) != page_h_pgno (&cur));

        test_err_t_wrap (pgr_release (f.p, &cur, PG_INNER_NODE, &f.e), &f.e);
        test_err_t_wrap (pgr_release (f.p, &cnext, PG_INNER_NODE, &f.e), &f.e);
      }
    }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

/////////////////////////////////////////
/// BALANCING NODES

void
dlgt_balance_with_prev (page_h *prev, page_h *cur)
{
  ASSERT (prev->mode == PHM_X);
  ASSERT (cur->mode == PHM_X);
  ASSERT (dlgt_valid_neighbors (page_h_ro (prev), page_h_ro (cur)));

  p_size prev_len = dlgt_get_len (page_h_ro (prev));
  p_size cur_len = dlgt_get_len (page_h_ro (cur));
  p_size maxlen = dlgt_get_max_len (page_h_ro (prev));

  // Already valid
  if (cur_len == 0)
    {
      return;
    }

  if (cur_len >= maxlen / 2)
    {
      return;
    }

  if (prev_len + cur_len >= maxlen)
    {
      dlgt_move_right (page_h_w (prev), page_h_w (cur), maxlen / 2 - cur_len);
      return;
    }
  else
    {
      dlgt_move_left (page_h_w (prev), page_h_w (cur), cur_len);
      return;
    }
}

TEST (TT_UNIT, dlgt_balance_with_prev)
{
  struct pgr_fixture f;
  error *e = &f.e;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);

  u8 _prev[DL_DATA_SIZE];
  u8 _cur[DL_DATA_SIZE];
  u32_arr_rand (_prev);
  u32_arr_rand (_cur);

  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  struct page_tree_builder builder = {
    .root = {
        .type = PG_INNER_NODE,
        .out = page_h_create (),
        .inner = {
            .dclen = 2,
            .clen = 2,
            .children = (struct page_desc[]){
                {
                    .type = PG_DATA_LIST,
                    .out = page_h_create (),
                    .size = DL_DATA_SIZE,
                    .data_list = {
                        .data = _prev,
                        .blen = DL_DATA_SIZE,
                    },
                },
                {
                    .type = PG_DATA_LIST,
                    .out = page_h_create (),
                    .size = DL_DATA_SIZE,
                    .data_list = {
                        .data = _cur,
                        .blen = DL_DATA_SIZE,
                    },
                },
            },
        },
    },
    .pager = f.p,
    .txn = &tx,
  };

  test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

  page_h *prev = &builder.root.inner.children[0].out;
  page_h *cur = &builder.root.inner.children[1].out;

  test_err_t_wrap (pgr_release (f.p, &builder.root.out, PG_INNER_NODE, e), e);

  TEST_CASE ("Both Full no change")
  {
    dlgt_balance_with_prev (prev, cur);
    test_assert_equal (dl_used (page_h_ro (prev)), DL_DATA_SIZE);
    test_assert_equal (dl_used (page_h_ro (cur)), DL_DATA_SIZE);
    test_assert_memequal (dl_get_data (page_h_ro (prev)), _prev, DL_DATA_SIZE);
    test_assert_memequal (dl_get_data (page_h_ro (cur)), _cur, DL_DATA_SIZE);
  }

  /**
   * BEFORE
   * [++++++++++++|****___10____]
   * [+++10+++____|_____________]
   *
   * AFTER
   * [++++++++++++|_____________]
   * [****++++++++|_____________]
   */
  TEST_CASE ("No Delete")
  {
    dl_memset (page_h_w (prev), _prev, DL_DATA_SIZE - 10);
    dl_memset (page_h_w (cur), _cur, 10);

    dlgt_balance_with_prev (prev, cur);
    test_assert_equal (dl_used (page_h_ro (prev)), DL_DATA_SIZE / 2 + DL_REM);
    test_assert_equal (dl_used (page_h_ro (cur)), DL_DATA_SIZE / 2);

    u32 i = 0;
    for (; i < DL_DATA_SIZE / 2 + DL_REM; ++i)
      {
        test_assert_equal (dl_get_byte (page_h_ro (prev), i), _prev[i]);
      }
    i = 0;
    for (; i < DL_DATA_SIZE - 10 - DL_DATA_SIZE / 2 - DL_REM; ++i)
      {
        test_assert_equal (dl_get_byte (page_h_ro (cur), i), _prev[DL_DATA_SIZE / 2 + DL_REM + i]);
      }
    u32 k = i;
    for (; i < DL_DATA_SIZE / 2; ++i)
      {
        test_assert_equal (dl_get_byte (page_h_ro (cur), i), _cur[i - k]);
      }
  }

  /**
   * BEFORE
   * [++++++++++++|++++___10____]
   * [***9***____|_____________]
   *
   * AFTER
   * [++++++++++++|++++***9***_]
   * [____________|_____________]
   */
  TEST_CASE ("Delete")
  {
    dl_memset (page_h_w (prev), _prev, DL_DATA_SIZE - 10);
    dl_memset (page_h_w (cur), _cur, 9);

    dlgt_balance_with_prev (prev, cur);
    test_assert_equal (dl_used (page_h_ro (prev)), DL_DATA_SIZE - 1);
    test_assert_equal (dl_used (page_h_ro (cur)), 0);

    u32 i = 0;
    // next data
    for (; i < DL_DATA_SIZE - 10; ++i)
      {
        test_assert_equal (dl_get_byte (page_h_ro (prev), i), _prev[i]);
      }
    u32 k = i;
    for (; i < 9; ++i)
      {
        test_assert_equal (dl_get_byte (page_h_ro (prev), i), _cur[i - k]);
      }
  }

  test_err_t_wrap (pgr_release (f.p, prev, PG_DATA_LIST, e), e);
  test_err_t_wrap (pgr_delete_and_release (f.p, &tx, cur, e), e);

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

void
dlgt_balance_with_next (page_h *cur, page_h *next)
{
  ASSERT (cur->mode == PHM_X);
  ASSERT (next->mode == PHM_X);
  ASSERT (dlgt_valid_neighbors (page_h_ro (cur), page_h_ro (next)));

  p_size next_len = dlgt_get_len (page_h_ro (next));
  p_size cur_len = dlgt_get_len (page_h_ro (cur));
  p_size maxlen = dlgt_get_max_len (page_h_ro (next));

  // Already valid
  if (cur_len == 0)
    {
      return;
    }

  if (cur_len >= maxlen / 2)
    {
      return;
    }

  if (next_len + cur_len >= maxlen)
    {
      dlgt_move_left (page_h_w (cur), page_h_w (next), maxlen / 2 - cur_len);
      return;
    }
  else
    {
      dlgt_move_right (page_h_w (cur), page_h_w (next), cur_len);
      return;
    }
}

TEST (TT_UNIT, dlgt_balance_with_next)
{
  struct pgr_fixture f;
  error *e = &f.e;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);

  u8 _cur[DL_DATA_SIZE];
  u8 _next[DL_DATA_SIZE];
  u32_arr_rand (_next);
  u32_arr_rand (_cur);

  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  struct page_tree_builder builder = {
    .root = {
        .type = PG_INNER_NODE,
        .out = page_h_create (),
        .inner = {
            .dclen = 2,
            .clen = 2,
            .children = (struct page_desc[]){
                {
                    .type = PG_DATA_LIST,
                    .out = page_h_create (),
                    .size = DL_DATA_SIZE,
                    .data_list = {
                        .data = _cur,
                        .blen = DL_DATA_SIZE,
                    },
                },
                {
                    .type = PG_DATA_LIST,
                    .out = page_h_create (),
                    .size = DL_DATA_SIZE,
                    .data_list = {
                        .data = _next,
                        .blen = DL_DATA_SIZE,
                    },
                },
            },
        },
    },
    .pager = f.p,
    .txn = &tx,
  };

  test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

  page_h *cur = &builder.root.inner.children[0].out;
  page_h *next = &builder.root.inner.children[1].out;

  test_err_t_wrap (pgr_release (f.p, &builder.root.out, PG_INNER_NODE, e), e);

  TEST_CASE ("Both Full no change")
  {
    dlgt_balance_with_next (cur, next);
    test_assert_equal (dl_used (page_h_ro (cur)), DL_DATA_SIZE);
    test_assert_equal (dl_used (page_h_ro (next)), DL_DATA_SIZE);
    test_assert_memequal (dl_get_data (page_h_ro (cur)), _cur, DL_DATA_SIZE);
    test_assert_memequal (dl_get_data (page_h_ro (next)), _next, DL_DATA_SIZE);
  }

  /**
   * BEFORE
   * [+++10+++____|_____________]
   * [****++++++++|++++___10____]
   *
   * AFTER
   * [+++10+++****|_____________]
   * [++++++++++++|___10____]
   */
  TEST_CASE ("No Delete")
  {
    _Static_assert(DL_DATA_SIZE > 10, "This test needs DL_DATA_SIZE > 10");
    dl_memset (page_h_w (cur), _cur, 10);
    dl_memset (page_h_w (next), _next, DL_DATA_SIZE - 10);

    dlgt_balance_with_next (cur, next);
    test_assert_equal (dl_used (page_h_ro (cur)), DL_DATA_SIZE / 2);
    test_assert_equal (dl_used (page_h_ro (next)), DL_DATA_SIZE / 2 + DL_REM);

    u32 i = 0;
    for (; i < 10; ++i)
      {
        test_assert_equal (dl_get_byte (page_h_ro (cur), i), _cur[i]);
      }
    for (; i < DL_DATA_SIZE / 2; ++i)
      {
        test_assert_equal (dl_get_byte (page_h_ro (cur), i), _next[i - 10]);
      }
    i = 0;
    for (; i < DL_DATA_SIZE / 2 + DL_REM; ++i)
      {
        test_assert_equal (dl_get_byte (page_h_ro (next), i), _next[i + DL_DATA_SIZE / 2 - 10]);
      }
  }

  /**
   * BEFORE
   * [+++10+++____|_____________]
   * [****++++++++|++++___10____]
   *
   * AFTER
   * [+++10+++****|_____________]
   * [++++++++++++|___10____]
   */
  TEST_CASE ("Delete")
  {
    dl_memset (page_h_w (cur), _cur, 9);
    dl_memset (page_h_w (next), _next, DL_DATA_SIZE - 10);

    dlgt_balance_with_next (cur, next);
    test_assert_equal (dl_used (page_h_ro (cur)), 0);
    test_assert_equal (dl_used (page_h_ro (next)), DL_DATA_SIZE - 1);

    u32 i = 0;
    for (; i < 9; ++i)
      {
        test_assert_equal (dl_get_byte (page_h_ro (next), i), _cur[i]);
      }

    // next data
    for (; i < DL_DATA_SIZE - 1; ++i)
      {
        test_assert_equal (dl_get_byte (page_h_ro (next), i), _next[i - 9]);
      }
  }

  test_err_t_wrap (pgr_release (f.p, next, PG_DATA_LIST, e), e);
  test_err_t_wrap (pgr_delete_and_release (f.p, &tx, cur, e), e);

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}
