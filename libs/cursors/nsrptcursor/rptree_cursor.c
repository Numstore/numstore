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
 *   Implements rptree_cursor.h. Provides core cursor management functions including
 *   initialization, cleanup, transaction handling, tree balancing, and validation.
 */

#include <numstore/rptree/rptree_cursor.h>

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/types.h>
#include <numstore/pager.h>
#include <numstore/pager/data_list.h>
#include <numstore/pager/inner_node.h>
#include <numstore/pager/page.h>
#include <numstore/pager/page_delegate.h>
#include <numstore/pager/page_h.h>
#include <numstore/pager/pager_routines.h>
#include <numstore/pager/rpt_root.h>
#include <numstore/test/page_fixture.h>
#include <numstore/test/testing.h>

#define VTYPE u8
#define KTYPE pgno
#define SUFFIX rptc_in
#include <numstore/core/robin_hood_ht.h>
#undef VTYPE
#undef KTYPE
#undef SUFFIX

static inline err_t
rptc_set_root (struct rptree_cursor *r, pgno root, error *e)
{
  return SUCCESS;
}

err_t
rptc_load_new_root (struct rptree_cursor *r, error *e)
{
  ASSERT (r->tx);

  page_h root = page_h_create ();
  err_t_wrap (pgr_new (&root, r->pager, r->tx, PG_INNER_NODE, e), e);

  in_init_empty (page_h_w (&root));

  ASSERT (r->cur.mode == PHM_NONE);

  // Update meta root root
  page_h dest = page_h_create ();
  err_t_wrap (pgr_get_writable (&dest, r->tx, PG_RPT_ROOT, r->meta_root, r->pager, e), e);
  rr_set_root (page_h_w (&dest), page_h_pgno (&root));
  err_t_wrap (pgr_release (r->pager, &dest, PG_RPT_ROOT, e), e);

  r->root = page_h_pgno (&root);
  r->cur = page_h_xfer_ownership (&root);
  r->lidx = 0;
  i_log_trace ("Created new layer with new root %" PRpgno "\n", page_h_pgno (&root));

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, rptc_load_new_root)
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

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}
#endif

err_t
rptc_pop_all (struct rptree_cursor *r, error *e)
{
  while (r->stack_state.sp != 0)
    {
      err_t_wrap (rptc_seek_pop_into_cur (r, e), e);
      err_t_wrap (pgr_release (r->pager, &r->cur, PG_INNER_NODE, e), e);
    }
  return SUCCESS;
}

static inline struct three_in_pair
three_in_pair_from (page_h *prev, page_h *cur, page_h *next)
{
  ASSERT (prev == NULL || prev->mode != PHM_NONE);
  ASSERT (cur == NULL || cur->mode != PHM_NONE);
  ASSERT (next == NULL || next->mode != PHM_NONE);

  struct three_in_pair ret = {
    .prev = in_pair_empty,
    .cur = in_pair_empty,
    .next = in_pair_empty,
  };

  if (prev)
    {
      ret.prev = in_pair_from_pgh (prev);
    }
  if (cur)
    {
      ret.cur = in_pair_from_pgh (cur);
    }
  if (next)
    {
      ret.next = in_pair_from_pgh (next);
    }

  return ret;
}

err_t
rptc_balance_and_release (
    struct three_in_pair *output,
    struct root_update *root,
    struct rptree_cursor *r,
    page_h *prev,
    page_h *next,
    error *e)
{
  ASSERT (prev->mode == PHM_NONE || dlgt_valid_neighbors (page_h_ro (prev), page_h_ro (&r->cur)));
  ASSERT (next->mode == PHM_NONE || dlgt_valid_neighbors (page_h_ro (&r->cur), page_h_ro (next)));
  ASSERT (output);

  err_t_wrap (pgr_maybe_make_writable (r->pager, r->tx, &r->cur, e), e);
  p_size csize = dlgt_get_len (page_h_ro (&r->cur));

  *output = three_in_pair_from (NULL, &r->cur, NULL);

  // Needs rebalancing
  if (csize > 0 && csize < dlgt_get_max_len (page_h_ro (&r->cur)) / 2)
    {
      TEST_MARKER ("rptree_cursor balance");

      // Try to use what we've got
      if (next->mode != PHM_NONE)
        {
          TEST_MARKER ("rptree_cursor balance with existing next");

          err_t_wrap (pgr_maybe_make_writable (r->pager, r->tx, next, e), e);
          dlgt_balance_with_next (&r->cur, next);
          *output = three_in_pair_from (NULL, &r->cur, next);
        }
      else if (prev->mode != PHM_NONE)
        {
          TEST_MARKER ("rptree_cursor balance with existing prev");

          err_t_wrap (pgr_maybe_make_writable (r->pager, r->tx, prev, e), e);
          dlgt_balance_with_prev (prev, &r->cur);
          *output = three_in_pair_from (prev, &r->cur, NULL);
        }

      // Load a new page
      else if (dlgt_get_next (page_h_ro (&r->cur)) != PGNO_NULL)
        {
          TEST_MARKER ("rptree_cursor balance with loaded next");

          err_t_wrap (pgr_dlgt_get_next (&r->cur, next, r->tx, r->pager, e), e);
          dlgt_balance_with_next (&r->cur, next);
          *output = three_in_pair_from (NULL, &r->cur, next);
        }
      else if (dlgt_get_prev (page_h_ro (&r->cur)) != PGNO_NULL)
        {
          TEST_MARKER ("rptree_cursor balance with loaded prev");

          err_t_wrap (pgr_dlgt_get_prev (&r->cur, prev, r->tx, r->pager, e), e);
          dlgt_balance_with_prev (prev, &r->cur);
          *output = three_in_pair_from (prev, &r->cur, NULL);
        }
      else
        {
          TEST_MARKER ("rptree_cursor balance performed on root");
        }
    }
  else
    {
      TEST_MARKER ("rptree_cursor no balance");
    }

  root->isroot = false;
  if (dlgt_is_root (page_h_ro (&r->cur)))
    {
      root->isroot = true;
      root->root = page_h_pgno (&r->cur);
    }

  // Need to delete cur
  if (dlgt_get_len (page_h_ro (&r->cur)) == 0)
    {
      i_log_trace ("Balance deleting: %" PRpgno "\n", page_h_pgno (&r->cur));

      // Fetch prev and next for link re writing
      if (!root->isroot)
        {
          if (prev->mode == PHM_NONE)
            {
              err_t_wrap (pgr_dlgt_get_prev (&r->cur, prev, r->tx, r->pager, e), e);
            }
          if (next->mode == PHM_NONE)
            {
              err_t_wrap (pgr_dlgt_get_next (&r->cur, next, r->tx, r->pager, e), e);
            }
          dlgt_link (page_h_w_or_null (prev), page_h_w_or_null (next));

          /**
           * Might have turned prev / next into a new root by deleting cur
           */
          if (prev->mode != PHM_NONE && dlgt_is_root (page_h_ro (prev)))
            {
              root->root = page_h_pgno (prev);
              root->isroot = true;
            }
          else if (next->mode != PHM_NONE && dlgt_is_root (page_h_ro (next)))
            {
              root->root = page_h_pgno (next);
              root->isroot = true;
            }
        }

      // Otherwise cur is still root but we will delete it so now it's NULL
      else
        {
          TEST_MARKER ("rptree_cursor balance performed on root and deleted");
          root->root = PGNO_NULL;
        }

      err_t_wrap (pgr_delete_and_release (r->pager, r->tx, &r->cur, e), e);
    }

  // One final common cleanup
  err_t_wrap (pgr_release_if_exists (r->pager, prev, PG_DATA_LIST | PG_INNER_NODE, e), e);
  err_t_wrap (pgr_release_if_exists (r->pager, &r->cur, PG_DATA_LIST | PG_INNER_NODE, e), e);
  err_t_wrap (pgr_release_if_exists (r->pager, next, PG_DATA_LIST | PG_INNER_NODE, e), e);

  return SUCCESS;
}

#ifndef NTEST
struct page_balance_release_test_params
{
  struct pgr_fixture *f;

  p_size plen;
  p_size clen;
  p_size nlen;

  b_size psize;
  b_size csize;
  b_size nsize;

  bool prev_present_before;
  bool next_present_before;
  struct txn *tx;

  // Expected result
  p_size exp_plen;
  p_size exp_clen;
  p_size exp_nlen;
  struct root_update exp_root;
};

static inline void
page_balance_release_test_case (struct page_balance_release_test_params params)
{
  struct page_tree_builder builder = in3in (params.f->p, params.tx, 3, params.plen, params.clen, params.nlen);

  page_h prev = page_h_create ();
  page_h cur = page_h_create ();
  page_h next = page_h_create ();

  pgno pgs[3] = { 0, 0, 0 };

  struct in_pair pdata[IN_MAX_KEYS];
  struct in_pair cdata[IN_MAX_KEYS];
  struct in_pair ndata[IN_MAX_KEYS];

  u32 pg = 0;
  for (p_size i = 0; i < IN_MAX_KEYS; ++i, ++pg)
    {
      pdata[i].pg = pg;
      pdata[i].key = 100;
    }
  for (p_size i = 0; i < IN_MAX_KEYS; ++i, ++pg)
    {
      cdata[i].pg = pg;
      cdata[i].key = 100;
    }
  for (p_size i = 0; i < IN_MAX_KEYS; ++i, ++pg)
    {
      ndata[i].pg = pg;
      ndata[i].key = 100;
    }

  page_h *prev_ptr = NULL;
  if (params.plen > 0)
    {
      test_err_t_wrap (build_fake_inner_node (
                           &prev, (struct in_page_builder){
                                      .pager = params.f->p,
                                      .txn = params.tx,
                                      .prev = prev_ptr,
                                      .next = NULL,
                                      .children = { .nodes = pdata, .len = params.plen },
                                      .dclen = params.plen,
                                  },
                           &params.f->e),
                       &params.f->e);
      prev_ptr = &prev;
      pgs[0] = page_h_pgno (&prev);
    }

  test_err_t_wrap (build_fake_inner_node (
                       &cur, (struct in_page_builder){
                                 .pager = params.f->p,
                                 .txn = params.tx,
                                 .prev = prev_ptr,
                                 .next = NULL,
                                 .children = { .nodes = cdata, .len = params.clen },
                                 .dclen = params.clen,
                             },
                       &params.f->e),
                   &params.f->e);
  prev_ptr = &cur;
  pgs[1] = page_h_pgno (&cur);

  if (params.nlen > 0)
    {
      test_err_t_wrap (build_fake_inner_node (
                           &next, (struct in_page_builder){
                                      .pager = params.f->p,
                                      .txn = params.tx,
                                      .prev = prev_ptr,
                                      .next = NULL,
                                      .children = { .nodes = ndata, .len = params.nlen },
                                      .dclen = params.nlen,
                                  },
                           &params.f->e),
                       &params.f->e);
      prev_ptr = &next;
      pgs[2] = page_h_pgno (&next);
    }

  if (params.plen > 0 && !params.prev_present_before)
    {
      test_err_t_wrap (pgr_release (params.f->p, &prev, PG_INNER_NODE, &params.f->e), &params.f->e);
    }

  if (params.nlen > 0 && !params.prev_present_before)
    {
      test_err_t_wrap (pgr_release (params.f->p, &next, PG_INNER_NODE, &params.f->e), &params.f->e);
    }

  struct rptree_cursor r = {
    .cur = page_h_xfer_ownership (&cur),
    .pager = params.f->p,
    .tx = params.tx,
  };

  struct three_in_pair output;
  struct root_update root;

  test_err_t_wrap (rptc_balance_and_release (&output, &root, &r, &prev, &next, &params.f->e), &params.f->e);

  if (params.exp_plen == 0)
    {
      test_assert (in_pair_is_empty (output.prev));
    }
  else
    {
      test_assert (!in_pair_is_empty (output.prev));
      test_assert (output.prev.key > 0);
      test_assert_type_equal (output.prev.pg, pgs[0], b_size, PRb_size);
      test_err_t_wrap (pgr_get (&prev, PG_INNER_NODE, pgs[0], params.f->p, &params.f->e), &params.f->e);
      test_assert_type_equal (in_get_len (page_h_ro (&prev)), params.exp_plen, p_size, PRp_size);
      test_err_t_wrap (pgr_release (params.f->p, &prev, PG_INNER_NODE, &params.f->e), &params.f->e);
    }

  if (params.exp_clen == 0)
    {
      test_assert_type_equal (output.cur.pg, pgs[1], pgno, PRpgno);
      test_assert_type_equal (output.cur.key, 0, b_size, PRb_size);
    }
  else
    {
      test_assert (!in_pair_is_empty (output.cur));
      test_assert (output.cur.key > 0);
      test_assert_type_equal (output.cur.pg, pgs[1], b_size, PRb_size);
      test_err_t_wrap (pgr_get (&cur, PG_INNER_NODE, pgs[1], params.f->p, &params.f->e), &params.f->e);
      test_assert_type_equal (in_get_len (page_h_ro (&cur)), params.exp_clen, p_size, PRp_size);
      test_err_t_wrap (pgr_release (params.f->p, &cur, PG_INNER_NODE, &params.f->e), &params.f->e);
    }

  if (params.exp_nlen == 0)
    {
      test_assert (in_pair_is_empty (output.next));
    }
  else
    {
      test_assert (!in_pair_is_empty (output.next));
      test_assert (output.next.key > 0);
      test_assert_type_equal (output.next.pg, pgs[2], b_size, PRb_size);
      test_err_t_wrap (pgr_get (&next, PG_INNER_NODE, pgs[2], params.f->p, &params.f->e), &params.f->e);
      test_assert_type_equal (in_get_len (page_h_ro (&next)), params.exp_nlen, p_size, PRp_size);
      test_err_t_wrap (pgr_release (params.f->p, &next, PG_INNER_NODE, &params.f->e), &params.f->e);
    }

  test_assert_int_equal (root.isroot, params.exp_root.isroot);

  if (root.isroot)
    {
      if (params.exp_root.root == PGNO_NULL)
        {
          test_assert_type_equal (root.root, params.exp_root.root, pgno, PRpgno);
        }
      else
        {
          test_assert_type_equal (root.root, pgs[params.exp_root.root], pgno, PRpgno);
        }
    }
}

TEST (TT_UNIT, rptc_balance_and_release)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);

  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  // ========== NO BALANCE CASES ==========

  TEST_CASE ("no balance root")
  {
    page_balance_release_test_case ((struct page_balance_release_test_params){
        .f = &f,
        .plen = 0,
        .clen = IN_MAX_KEYS / 2,
        .nlen = 0,
        .tx = &tx,

        .exp_plen = 0,
        .exp_clen = IN_MAX_KEYS / 2,
        .exp_nlen = 0,

        .exp_root = (struct root_update){
            .isroot = true,
            .root = 1,
        },
    });
  }

  TEST_CASE ("no balance non-root")
  {
    page_balance_release_test_case ((struct page_balance_release_test_params){
        .f = &f,
        .plen = IN_MAX_KEYS / 2,
        .clen = IN_MAX_KEYS / 2,
        .nlen = 0,
        .tx = &tx,

        .exp_plen = 0,
        .exp_clen = IN_MAX_KEYS / 2,
        .exp_nlen = 0,

        .exp_root = (struct root_update){
            .isroot = false,
        },
    });
  }

  // ========== BALANCE WITH EXISTING PREV ==========

  TEST_CASE ("balance existing prev keep both")
  {
    page_balance_release_test_case ((struct page_balance_release_test_params){
        .f = &f,
        .plen = IN_MAX_KEYS / 2 + 2,
        .clen = IN_MAX_KEYS / 2 - 1,
        .nlen = 0,
        .tx = &tx,
        .prev_present_before = true,

        .exp_plen = IN_MAX_KEYS / 2 + 1,
        .exp_clen = IN_MAX_KEYS / 2,
        .exp_nlen = 0,

        .exp_root = (struct root_update){
            .isroot = false,
        },
    });
  }

  TEST_CASE ("balance existing prev delete cur")
  {
    page_balance_release_test_case ((struct page_balance_release_test_params){
        .f = &f,
        .plen = IN_MAX_KEYS / 2,
        .clen = 1,
        .nlen = 0,
        .tx = &tx,
        .prev_present_before = true,

        .exp_plen = IN_MAX_KEYS / 2 + 1,
        .exp_clen = 0,
        .exp_nlen = 0,

        .exp_root = (struct root_update){
            .isroot = true,
            .root = 0,
        },
    });
  }

  // ========== BALANCE WITH EXISTING NEXT ==========

  TEST_CASE ("balance existing next keep both")
  {
    page_balance_release_test_case ((struct page_balance_release_test_params){
        .f = &f,
        .plen = 0,
        .clen = IN_MAX_KEYS / 2 - 1,
        .nlen = IN_MAX_KEYS / 2 + 2,
        .tx = &tx,
        .next_present_before = true,

        .exp_plen = 0,
        .exp_clen = IN_MAX_KEYS / 2,
        .exp_nlen = IN_MAX_KEYS / 2 + 1,

        .exp_root = (struct root_update){
            .isroot = false,
        },
    });
  }

  TEST_CASE ("balance existing next delete cur")
  {
    page_balance_release_test_case ((struct page_balance_release_test_params){
        .f = &f,
        .plen = 0,
        .clen = 1,
        .nlen = IN_MAX_KEYS / 2,
        .tx = &tx,
        .next_present_before = true,

        .exp_plen = 0,
        .exp_clen = 0,
        .exp_nlen = IN_MAX_KEYS / 2 + 1,

        .exp_root = (struct root_update){
            .isroot = true,
            .root = 2,
        },
    });
  }

  // ========== BALANCE WITH LOADED PREV ==========

  TEST_CASE ("balance loaded prev keep both")
  {
    page_balance_release_test_case ((struct page_balance_release_test_params){
        .f = &f,
        .plen = IN_MAX_KEYS / 2 + 2,
        .clen = IN_MAX_KEYS / 2 - 1,
        .nlen = 0,
        .tx = &tx,
        .prev_present_before = false,

        .exp_plen = IN_MAX_KEYS / 2 + 1,
        .exp_clen = IN_MAX_KEYS / 2,
        .exp_nlen = 0,

        .exp_root = (struct root_update){
            .isroot = false,
        },
    });
  }

  TEST_CASE ("balance loaded prev delete cur")
  {
    page_balance_release_test_case ((struct page_balance_release_test_params){
        .f = &f,
        .plen = IN_MAX_KEYS / 2,
        .clen = IN_MAX_KEYS / 2 - 1,
        .nlen = 0,
        .tx = &tx,
        .prev_present_before = false,

        .exp_plen = IN_MAX_KEYS / 2 + IN_MAX_KEYS / 2 - 1,
        .exp_clen = 0,
        .exp_nlen = 0,

        .exp_root = (struct root_update){
            .isroot = true,
            .root = 0,
        },
    });
  }

  // ========== BALANCE WITH LOADED NEXT ==========

  TEST_CASE ("balance loaded next keep both")
  {
    page_balance_release_test_case ((struct page_balance_release_test_params){
        .f = &f,
        .plen = 0,
        .clen = IN_MAX_KEYS / 2 - 1,
        .nlen = IN_MAX_KEYS / 2 + 2,
        .tx = &tx,
        .next_present_before = false,

        .exp_plen = 0,
        .exp_clen = IN_MAX_KEYS / 2,
        .exp_nlen = IN_MAX_KEYS / 2 + 1,

        .exp_root = (struct root_update){
            .isroot = false,
        },
    });
  }

  TEST_CASE ("balance loaded next delete cur")
  {
    page_balance_release_test_case ((struct page_balance_release_test_params){
        .f = &f,
        .plen = 0,
        .clen = IN_MAX_KEYS / 2 - 1,
        .nlen = IN_MAX_KEYS / 2,
        .tx = &tx,
        .next_present_before = false,

        .exp_plen = 0,
        .exp_clen = 0,
        .exp_nlen = IN_MAX_KEYS / 2 + IN_MAX_KEYS / 2 - 1,

        .exp_root = (struct root_update){
            .isroot = true,
            .root = 2,
        },
    });
  }

  // ========== BALANCE ROOT ==========

  TEST_CASE ("balance root")
  {
    page_balance_release_test_case ((struct page_balance_release_test_params){
        .f = &f,
        .plen = 0,
        .clen = IN_MAX_KEYS / 2 - 1,
        .nlen = 0,
        .tx = &tx,

        .exp_plen = 0,
        .exp_clen = IN_MAX_KEYS / 2 - 1,
        .exp_nlen = 0,

        .exp_root = (struct root_update){
            .isroot = true,
            .root = 1,
        },
    });
  }

  // ========== DELETE CASES ==========

  TEST_CASE ("delete empty root")
  {
    page_balance_release_test_case ((struct page_balance_release_test_params){
        .f = &f,
        .plen = 0,
        .clen = 0,
        .nlen = 0,
        .tx = &tx,

        .exp_plen = 0,
        .exp_clen = 0,
        .exp_nlen = 0,

        .exp_root = (struct root_update){
            .isroot = true,
            .root = PGNO_NULL,
        },
    });
  }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

#endif

void
rptc_enter_transaction (struct rptree_cursor *r, struct txn *tx)
{
  latch_lock (&r->latch);

  DBG_ASSERT (rptc_unseeked, r);
  ASSERT (r->tx == NULL || r->tx == tx);
  r->tx = tx;

  latch_unlock (&r->latch);
}

void
rptc_leave_transaction (struct rptree_cursor *r)
{
  latch_lock (&r->latch);

  DBG_ASSERT (rptc_unseeked, r);
  ASSERT (r->tx);
  r->tx = NULL;

  latch_unlock (&r->latch);
}

err_t
rptc_open (struct rptree_cursor *r, pgno root, struct pager *p, error *e)
{
  r->pager = p;
  r->meta_root = root;
  r->tx = NULL;
  r->cur = page_h_create ();
  r->lidx = 0;
  r->stack_state.sp = 0;
  r->state = RPTS_UNSEEKED;
  latch_init (&r->latch);

  // Fetch root page
  page_h root_pg = page_h_create ();
  err_t_wrap (pgr_get (&root_pg, PG_RPT_ROOT, root, p, e), e);
  r->root = rr_get_root (page_h_ro (&root_pg));
  err_t_wrap (pgr_release (r->pager, &root_pg, PG_RPT_ROOT, e), e);

  // Fetch data size
  if (r->root != PGNO_NULL)
    {
      err_t_wrap (pgr_get (&root_pg, PG_INNER_NODE | PG_DATA_LIST, r->root, p, e), e);
      r->total_size = dlgt_get_size (page_h_ro (&root_pg));
      err_t_wrap (pgr_release (r->pager, &root_pg, page_h_type (&root_pg), e), e);
    }
  else
    {
      r->total_size = 0;
    }

  DBG_ASSERT (rptc_unseeked, r);

  return SUCCESS;
}

err_t
rptc_new (struct rptree_cursor *r, struct txn *tx, struct pager *p, error *e)
{
  page_h root_pg = page_h_create ();
  err_t_wrap (pgr_new (&root_pg, p, tx, PG_RPT_ROOT, e), e);
  r->meta_root = page_h_ro (&root_pg)->pg;
  err_t_wrap (pgr_release (p, &root_pg, PG_RPT_ROOT, e), e);

  r->pager = p;
  r->root = PGNO_NULL;
  r->tx = tx;
  r->cur = page_h_create ();
  r->lidx = 0;

  r->stack_state.sp = 0;

  r->state = RPTS_UNSEEKED;
  latch_init (&r->latch);

  DBG_ASSERT (rptc_unseeked, r);

  return SUCCESS;
}

err_t
rptc_cleanup (struct rptree_cursor *r, error *e)
{
  DBG_ASSERT (rptc_unseeked, r);
  return e->cause_code;
}

#ifndef NTEST
TEST (TT_UNIT, rptree_cursor_init_teardown)
{
  struct pgr_fixture pf;
  struct rptree_cursor r;
  test_err_t_wrap (pgr_fixture_create (&pf), &pf.e);

  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, pf.p, &pf.e), &pf.e);
  test_err_t_wrap (rptc_new (&r, &tx, pf.p, &pf.e), &pf.e);
  test_err_t_wrap (rptc_cleanup (&r, &pf.e), &pf.e);

  test_err_t_wrap (pgr_commit (pf.p, &tx, &pf.e), &pf.e);

  test_err_t_wrap (pgr_fixture_teardown (&pf), &pf.e);
}
#endif

static err_t
rptc_validate_cur_layer (struct rptree_cursor *r, hash_table_rptc_in *table, page_h *cur, error *e)
{
  pgno cpg = page_h_pgno (cur);
  enum page_type ctype = page_h_type (cur);

  switch (ht_insert_rptc_in (table, (hdata_rptc_in){ .key = cpg, .value = 0 }))
    {
    case HTIR_EXISTS:
      {
        return error_causef (
            e, ERR_NOMEM,
            "Found duplicate key: %" PRpgno, cpg);
      }
    case HTIR_FULL:
      {
        return error_causef (
            e, ERR_RPTREE_INVALID,
            "No more room for rptc validation check. This is not a bug, "
            "consider increasing validation size");
      }
    case HTIR_SUCCESS:
      {
        break;
      }
    }

  // Normal page validation
  err_t_wrap (page_validate_for_db (page_h_ro (cur), page_h_type (cur), e), e);

  switch (page_h_type (cur))
    {
    case PG_INNER_NODE:
      {
        enum page_type type; // lazily set on first

        for (p_size i = 0; i < dlgt_get_len (page_h_ro (cur)); ++i)
          {
            // Fetch the first page
            page_h iter = page_h_create ();
            pgno pg = in_get_leaf (page_h_ro (cur), i);
            b_size key = in_get_key (page_h_ro (cur), i);
            if (key == 0)
              {
                return error_causef (
                    e, ERR_RPTREE_INVALID,
                    "Inner node with page number: %" PRpgno " contains a "
                    "0 key at index: %d (with leaf: %" PRpgno,
                    cpg, i, pg);
              }
            err_t_wrap (pgr_get (&iter, PG_DATA_LIST | PG_INNER_NODE, pg, r->pager, e), e);

            pgno eprev;
            pgno enext;

            if (i == 0)
              {
                type = page_h_type (&iter);
                if (dlgt_get_prev (page_h_ro (cur)) != PGNO_NULL)
                  {
                    page_h temp = page_h_create ();
                    err_t_wrap (pgr_get (&temp, ctype, dlgt_get_prev (page_h_ro (cur)), r->pager, e), e);
                    eprev = in_get_leaf (page_h_ro (&temp), dlgt_get_len (page_h_ro (&temp)) - 1);
                    err_t_wrap (pgr_release (r->pager, &temp, ctype, e), e);
                  }
                else
                  {
                    eprev = PGNO_NULL;
                  }
              }
            else
              {
                eprev = in_get_leaf (page_h_ro (cur), i - 1);
              }

            if (i + 1 < dlgt_get_len (page_h_ro (cur)))
              {
                enext = in_get_leaf (page_h_ro (cur), i + 1);
              }
            else
              {
                if (dlgt_get_next (page_h_ro (cur)) != PGNO_NULL)
                  {
                    page_h temp = page_h_create ();
                    err_t_wrap (pgr_get (&temp, ctype, dlgt_get_next (page_h_ro (cur)), r->pager, e), e);
                    enext = in_get_leaf (page_h_ro (&temp), 0);
                    err_t_wrap (pgr_release (r->pager, &temp, ctype, e), e);
                  }
                else
                  {
                    enext = PGNO_NULL;
                  }
              }

            // Check that types match
            if (page_h_type (&iter) != type)
              {
                return error_causef (
                    e, ERR_RPTREE_INVALID,
                    "Page of type: %d doesn't belong in layer of type: %d",
                    page_h_type (&iter), type);
              }

            // check that sizes match
            if (key != dlgt_get_size (page_h_ro (&iter)))
              {
                return error_causef (
                    e, ERR_RPTREE_INVALID,
                    "Actual page size: %" PRb_size " of "
                    "page: %" PRpgno " doesn't equal reported page "
                    "size: %" PRb_size " of inner node: %" PRpgno,
                    dlgt_get_size (page_h_ro (&iter)), pg, key, cpg);
              }

            // check that prev matches
            if (dlgt_get_prev (page_h_ro (&iter)) != eprev)
              {
                return error_causef (
                    e, ERR_RPTREE_INVALID,
                    "Actual page prev: %" PRpgno " of "
                    "page: %" PRpgno " doesn't equal reported page "
                    "prev: %" PRpgno " of inner node: %" PRpgno,
                    dlgt_get_prev (page_h_ro (&iter)), pg, eprev, cpg);
              }

            // check that next matches
            if (dlgt_get_next (page_h_ro (&iter)) != enext)
              {
                return error_causef (
                    e, ERR_RPTREE_INVALID,
                    "Actual page next: %" PRpgno " of "
                    "page: %" PRpgno " doesn't equal reported page "
                    "next: %" PRpgno " of inner node: %" PRpgno,
                    dlgt_get_next (page_h_ro (&iter)), pg, enext, cpg);
              }

            // release cur temporarily to save space
            err_t_wrap (pgr_release (r->pager, cur, ctype, e), e);

            // Validate child
            err_t_wrap (rptc_validate_cur_layer (r, table, &iter, e), e);
            err_t_wrap (pgr_release (r->pager, &iter, type, e), e);

            // Re fetch cur for the for loop
            err_t_wrap (pgr_get (cur, ctype, cpg, r->pager, e), e);
          }

        return SUCCESS;
      }
    case PG_DATA_LIST:
      {
        return SUCCESS;
      }
    default:
      {
        UNREACHABLE ();
      }
    }
}

err_t
rptc_validate (struct rptree_cursor *r, error *e)
{
  DBG_ASSERT (rptc_unseeked, r);

  if (r->root == PGNO_NULL)
    {
      return SUCCESS;
    }

  hash_table_rptc_in table;
  hentry_rptc_in *arr = i_malloc (204800, sizeof *arr, e);
  if (arr == 0)
    {
      return e->cause_code;
    }
  ht_init_rptc_in (&table, arr, 204800);

  page_h cur = page_h_create ();
  if (pgr_get (&cur, PG_INNER_NODE | PG_DATA_LIST, r->root, r->pager, e))
    {
      goto theend;
    }

  if (dlgt_get_prev (page_h_ro (&cur)) != PGNO_NULL || dlgt_get_next (page_h_ro (&cur)) != PGNO_NULL)
    {
      error_causef (
          e, ERR_RPTREE_INVALID,
          "Root page: %" PRpgno " should have null siblings, "
          "but has (prev, next) (%" PRpgno ", %" PRpgno ")",
          page_h_pgno (&cur),
          dlgt_get_prev (page_h_ro (&cur)),
          dlgt_get_next (page_h_ro (&cur)));
      goto theend;
    }

  if (rptc_validate_cur_layer (r, &table, &cur, e))
    {
      goto theend;
    }

theend:
  if (cur.mode != PHM_NONE)
    {
      pgr_release (r->pager, &cur, page_h_type (&cur), e);
    }
  i_free (arr);

  return e->cause_code;
}

TEST (TT_UNIT, rptc_validate)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  TEST_CASE ("stand")
  {
    struct page_tree_builder builder = in5dl (f.p, &tx, 5, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE, DL_DATA_SIZE);
    test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

    pgno rpg = page_h_pgno (&builder.root.out);

    test_err_t_wrap (page_tree_builder_release_all (&builder, &f.e), &f.e);

    struct rptree_cursor r;
    test_err_t_wrap (rptc_new (&r, &tx, f.p, &f.e), &f.e);
    r.root = rpg;

    test_err_t_wrap (rptc_validate (&r, &f.e), &f.e);
  }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}
