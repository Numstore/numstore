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
 *   Implements _seek.h. Provides seek operations to navigate to a specific
 *   byte offset within the R+ tree, traversing inner nodes to locate the
 *   target data list page.
 */

#include <numstore/core/error.h>
#include <numstore/core/math.h>
#include <numstore/intf/logging.h>
#include <numstore/pager.h>
#include <numstore/pager/pager_routines.h>
#include <numstore/rptree/rptree_cursor.h>
#include <numstore/test/page_fixture.h>
#include <numstore/test/testing.h>

#ifndef NTEST
#endif

DEFINE_DBG_ASSERT (
    struct rptree_cursor, rptc_seeking, r,
    {})

////////////////////////
/// SEEK

err_t
rptc_seek_pop_into_cur (struct rptree_cursor *r, error *e)
{
  struct seek_v *ref = &r->stack_state.stack[--r->stack_state.sp];

  struct seek_v v = {
    .pg = page_h_xfer_ownership (&ref->pg),
    .lidx = ref->lidx,
  };

  if (r->cur.mode != PHM_NONE)
    {
      err_t_wrap (pgr_release (r->pager, &r->cur, PG_INNER_NODE | PG_DATA_LIST, e), e);
    }

  r->cur = v.pg;
  r->lidx = v.lidx;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, rptc_seek_pop_into_cur)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);

  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  struct rptree_cursor r;
  test_err_t_wrap (rptc_new (&r, &tx, f.p, &f.e), &f.e);

  rptc_enter_transaction (&r, &tx);

  test_err_t_wrap (rptc_load_new_root (&r, &f.e), &f.e);

  test_assert_int_equal (r.cur.mode, PHM_X);
  test_assert_int_equal (r.root, page_h_pgno (&r.cur));
  test_assert_int_equal (r.lidx, 0);
  test_assert_int_equal (page_get_type (page_h_ro (&r.cur)), PG_INNER_NODE);

  test_err_t_wrap (pgr_delete_and_release (f.p, r.tx, &r.cur, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}
#endif

static void
rptc_seek_load_choice (struct rptree_cursor *r)
{
  DBG_ASSERT (rptc_seeking, r);
  ASSERT (r->state == RPTS_SEEKING);

  /**
   * lidx, gidx, seek.b are loaded
   */
  switch (page_h_type (&r->cur))
    {
    /**
     * An inner node - we must continue to seek
     */
    case PG_INNER_NODE:
      {
        b_size nleft;
        p_size lidx;
        in_choose_lidx (&lidx, &nleft, page_h_ro (&r->cur), r->seeker.remaining);
        r->lidx = lidx;

        ASSERT (nleft <= r->seeker.remaining);

        r->seeker.remaining -= nleft;

        i_log_trace ("================ Seek Choice Begin\n");
        i_log_trace ("Decision index: %" PRp_size "\n", r->lidx);
        i_log_trace ("Decision Value: %" PRpgno ", %" PRb_size "\n",
                     in_get_leaf (page_h_ro (&r->cur), r->lidx),
                     in_get_key (page_h_ro (&r->cur), r->lidx));
        i_log_trace ("NLeft: %" PRb_size "\n", nleft);
        i_log_trace ("Seek Remaining: %" PRb_size "\n", r->seek.b);
        i_log_trace ("================ Done\n");

        return;
      }

    /**
     * A data list - we just finished our seek
     */
    case PG_DATA_LIST:
      {
        p_size used = dl_used (page_h_ro (&r->cur));

        // Choose the in-page index: clip to [0, used]
        p_size take = MIN (r->seeker.remaining, used);

        r->lidx = take;

        // Seek is complete on a data list page
        r->seeker.remaining = 0;
        r->state = RPTS_SEEKED;

        i_log_trace ("============== Seek Complete Begin\n");
        i_log_trace ("Data List Node Page Number: %" PRpgno "\n", page_h_pgno (&r->cur));
        i_log_trace ("Lidx: %" PRsp_size "\n", r->lidx);
        i_log_trace ("Gidx: %" PRsb_size "\n", r->gidx);
        i_log_trace ("IsEof: %d\n", r->iseof);
        i_log_trace ("============== Seek Complete End\n");

        return;
      }
    default:
      {
        UNREACHABLE ();
      }
    }
}

#ifndef NTEST
typedef struct
{
  b_size gidxb;
  b_size bb;
  p_size lidxa;
  b_size gidxa;
  b_size ba;
  bool iseofa;
} rptc_seek_load_choice_test_case;

TEST (TT_UNIT, rptc_seek_load_choice)
{
  struct pgr_fixture f;
  error *e = &f.e;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, e), e);

  page_h cur = page_h_create ();
  test_err_t_wrap (pgr_new (&cur, f.p, &tx, PG_INNER_NODE, e), e);

  pgno pgs[] = { 0, 1, 2, 3, 4 };
  b_size keys[] = { 10, 15, 10, 1, 11 };
  struct in_pair pairs[5];

  struct in_data d = {
    .nodes = pairs,
    .len = 5,
  };

  in_data_from_arrays (&d, pgs, keys);
  in_set_data (page_h_w (&cur), d);

  struct rptree_cursor r = (struct rptree_cursor){
    .pager = f.p,
    .state = RPTS_SEEKING,
    .cur = page_h_xfer_ownership (&cur),
  };

  rptc_seek_load_choice_test_case in_cases[] = {
    // Some simple ones
    { .gidxb = 0, .bb = 5, .lidxa = 0, .gidxa = 0, .ba = 5, .iseofa = false },
    { .gidxb = 5, .bb = 5, .lidxa = 0, .gidxa = 5, .ba = 5, .iseofa = false },
    { .gidxb = 5, .bb = 10, .lidxa = 1, .gidxa = 15, .ba = 0, .iseofa = false },
    { .gidxb = 5, .bb = 10, .lidxa = 1, .gidxa = 15, .ba = 0, .iseofa = false },

    // b in first bucket
    { .gidxb = 0, .bb = 0, .lidxa = 0, .gidxa = 0, .ba = 0, .iseofa = false },
    { .gidxb = 0, .bb = 9, .lidxa = 0, .gidxa = 0, .ba = 9, .iseofa = false },

    // exact boundary at 10 -> enter second bucket
    { .gidxb = 0, .bb = 10, .lidxa = 1, .gidxa = 10, .ba = 0, .iseofa = false },

    // interior of second bucket
    { .gidxb = 0, .bb = 24, .lidxa = 1, .gidxa = 10, .ba = 14, .iseofa = false },

    // exact boundary at 25 -> third bucket
    { .gidxb = 0, .bb = 25, .lidxa = 2, .gidxa = 25, .ba = 0, .iseofa = false },

    // interior of third bucket
    { .gidxb = 0, .bb = 34, .lidxa = 2, .gidxa = 25, .ba = 9, .iseofa = false },

    // exact boundary at 35 -> fourth bucket (size 1)
    { .gidxb = 0, .bb = 35, .lidxa = 3, .gidxa = 35, .ba = 0, .iseofa = false },

    // exact boundary at 36 -> fifth bucket
    { .gidxb = 0, .bb = 36, .lidxa = 4, .gidxa = 36, .ba = 0, .iseofa = false },

    // interior of fifth bucket
    { .gidxb = 0, .bb = 46, .lidxa = 4, .gidxa = 36, .ba = 10, .iseofa = false },

    // Massive interior of fifth bucket
    { .gidxb = 0, .bb = 4600000, .lidxa = 4, .gidxa = 36, .ba = 4600000 - 36, .iseofa = false },

    // repeat a few with nonzero initial gidx to ensure additive behavior
    { .gidxb = 7, .bb = 10, .lidxa = 1, .gidxa = 17, .ba = 0, .iseofa = false },
    { .gidxb = 3, .bb = 34, .lidxa = 2, .gidxa = 28, .ba = 9, .iseofa = false },
    { .gidxb = 11, .bb = 36, .lidxa = 4, .gidxa = 47, .ba = 0, .iseofa = false },
  };

  for (u32 i = 0; i < arrlen (in_cases); ++i)
    {
      r.seeker.remaining = in_cases[i].bb;

      rptc_seek_load_choice (&r);

      test_assert_int_equal (r.lidx, in_cases[i].lidxa);
      test_assert_int_equal (r.seeker.remaining, in_cases[i].ba);
      test_assert_int_equal (r.state, RPTS_SEEKING);
    }

  test_err_t_wrap (pgr_release (f.p, &r.cur, PG_INNER_NODE, &f.e), &f.e);

  test_err_t_wrap (pgr_new (&cur, f.p, &tx, PG_DATA_LIST, e), e);
  dl_set_used (page_h_w (&cur), 10);

  r = (struct rptree_cursor){
    .pager = f.p,
    .state = RPTS_SEEKING,
    .cur = page_h_xfer_ownership (&cur),
  };

  rptc_seek_load_choice_test_case dl_cases[] = {
    // Within bounds
    { .gidxb = 0, .bb = 0, .lidxa = 0, .gidxa = 0, .ba = 0, .iseofa = false },
    { .gidxb = 0, .bb = 1, .lidxa = 1, .gidxa = 1, .ba = 0, .iseofa = false },
    { .gidxb = 2, .bb = 5, .lidxa = 5, .gidxa = 7, .ba = 0, .iseofa = false },
    { .gidxb = 10, .bb = 9, .lidxa = 9, .gidxa = 19, .ba = 0, .iseofa = false },

    // Boundary at used (10) -> clip right, eof
    { .gidxb = 0, .bb = 10, .lidxa = 10, .gidxa = 10, .ba = 0, .iseofa = true },

    // Exceeds used -> clip right, eof
    { .gidxb = 3, .bb = 11, .lidxa = 10, .gidxa = 13, .ba = 0, .iseofa = true },
    { .gidxb = 0, .bb = 42, .lidxa = 10, .gidxa = 10, .ba = 0, .iseofa = true },
  };

  for (u32 i = 0; i < arrlen (dl_cases); ++i)
    {
      r.seeker.remaining = dl_cases[i].bb;

      rptc_seek_load_choice (&r);

      test_assert_int_equal (r.lidx, dl_cases[i].lidxa);
      test_assert_int_equal (r.seeker.remaining, dl_cases[i].ba);
      test_assert_int_equal (r.state, RPTS_SEEKED);
      r.state = RPTS_SEEKING;
    }
  test_err_t_wrap (pgr_release (f.p, &r.cur, PG_DATA_LIST, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}
#endif

err_t
rptc_start_seek (
    struct rptree_cursor *r,
    b_size loc,
    bool newroot,
    error *e)
{
  DBG_ASSERT (rptc_unseeked, r);

  if (r->root == PGNO_NULL)
    {
      if (newroot)
        {
          ASSERT (r->tx);

          err_t_wrap (pgr_new (&r->cur, r->pager, r->tx, PG_DATA_LIST, e), e);
          r->root = page_h_pgno (&r->cur);
        }
      else
        {
          // Just stay unseeked
          return SUCCESS;
        }
    }
  else
    {
      err_t_wrap (pgr_get (&r->cur, PG_DATA_LIST | PG_INNER_NODE, r->root, r->pager, e), e);
    }

  r->seeker.remaining = loc;
  r->lidx = 0;

  r->state = RPTS_SEEKING;
  rptc_seek_load_choice (r);

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, rptc_start_seek)
{
  struct pgr_fixture f;
  struct rptree_cursor r;
  u32 dummy[DL_DATA_SIZE * 2];
  arr_range (dummy);
  struct cbuffer src;

  test_err_t_wrap (pgr_fixture_create (&f), &f.e);

  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  test_err_t_wrap (rptc_new (&r, &tx, f.p, &f.e), &f.e);

  // Seek to 0 on empty
  TEST_CASE ("Seek to location 0 on an empty rptree")
  {
    test_err_t_wrap (rptc_start_seek (&r, 0, false, &f.e), &f.e);
    test_assert_int_equal (r.state, RPTS_UNSEEKED);
  }

  // Seek to > 0 on empty
  TEST_CASE ("Seek to location > 0 on an empty rptree")
  {
    test_err_t_wrap (rptc_start_seek (&r, 100, false, &f.e), &f.e);
    test_assert_int_equal (r.state, RPTS_UNSEEKED);
  }

  TEST_CASE ("Seek to 0 then write and rebalance to top")
  {
    rptc_enter_transaction (&r, &tx);

    test_err_t_wrap (rptc_start_seek (&r, 0, true, &f.e), &f.e);
    test_assert_int_equal (r.state, RPTS_SEEKED);

    src = cbuffer_create_full_from (dummy);
    test_err_t_wrap (rptc_seeked_to_insert (&r, &src, 0, &f.e), &f.e);
    test_assert_int_equal (r.state, RPTS_DL_INSERTING);

    while (cbuffer_len (&src) > 0)
      {
        test_err_t_wrap (rptc_insert_execute (&r, &f.e), &f.e);
        test_assert_int_equal (r.state, RPTS_DL_INSERTING);
      }

    test_err_t_wrap (rptc_insert_to_rebalancing_or_unseeked (&r, &f.e), &f.e);
    test_assert_int_equal (r.state, RPTS_IN_REBALANCING);

    test_err_t_wrap (rptc_rebalance_execute (&r, &f.e), &f.e);
    test_err_t_wrap (rptc_rebalance_execute (&r, &f.e), &f.e);
    test_assert_int_equal (r.state, RPTS_UNSEEKED);
  }

  TEST_CASE ("Seek to 100 then write and rebalance to top")
  {
    test_err_t_wrap (rptc_start_seek (&r, 100 * sizeof (*dummy), false, &f.e), &f.e);
    test_assert_int_equal (r.state, RPTS_SEEKING);

    test_err_t_wrap (rptc_seeking_execute (&r, &f.e), &f.e);
    test_assert_int_equal (r.state, RPTS_SEEKED);

    src = cbuffer_create_full_from (dummy);
    test_err_t_wrap (rptc_seeked_to_insert (&r, &src, 0, &f.e), &f.e);
    test_assert_int_equal (r.state, RPTS_DL_INSERTING);

    while (cbuffer_len (&src) > 0)
      {
        test_err_t_wrap (rptc_insert_execute (&r, &f.e), &f.e);
        test_assert_int_equal (r.state, RPTS_DL_INSERTING);
      }

    test_err_t_wrap (rptc_insert_to_rebalancing_or_unseeked (&r, &f.e), &f.e);

    test_assert_int_equal (r.state, RPTS_UNSEEKED);
  }

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}
#endif

err_t
rptc_seeking_execute (struct rptree_cursor *r, error *e)
{
  DBG_ASSERT (rptc_seeking, r);
  ASSERT (r->state == RPTS_SEEKING);

  // Check for seek stack overflow
  if (r->stack_state.sp == 20)
    {
      return error_causef (
          e, ERR_RPTREE_PAGE_STACK_OVERFLOW,
          "Seek: Page stack overflow");
    }

  // Pre fetch the next page
  pgno npg = in_get_leaf (page_h_ro (&r->cur), r->lidx);
  page_h next = page_h_create ();
  err_t_wrap (pgr_get (&next, PG_DATA_LIST | PG_INNER_NODE, npg, r->pager, e), e);

  // Push current page to the top of the stack
  r->stack_state.stack[r->stack_state.sp++] = (struct seek_v){
    .pg = page_h_xfer_ownership (&r->cur),
    .lidx = r->lidx,
  };

  // Save this new page
  r->cur = page_h_xfer_ownership (&next);

  rptc_seek_load_choice (r);

  return SUCCESS;
}
