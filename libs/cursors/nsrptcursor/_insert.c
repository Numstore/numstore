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
 *   Implements _insert.h. Provides insertion operations for the R+ tree cursor,
 *   handling data insertion at the current cursor position with automatic page
 *   splitting and tree rebalancing.
 */

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/core/random.h>
#include <numstore/intf/logging.h>
#include <numstore/pager/data_list.h>
#include <numstore/pager/inner_node.h>
#include <numstore/pager/pager_routines.h>
#include <numstore/rptree/rptree_cursor.h>
#include <numstore/test/page_fixture.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    struct rptree_cursor, rptc_inserting, r,
    {
      ASSERT (r->tx);
      ASSERT (r->lidx == dl_used (page_h_ro (&r->cur)));
      ASSERT (r->inserter.max_write == 0 || r->inserter.total_written <= r->inserter.max_write);
    })

err_t
rptc_seeked_to_insert (struct rptree_cursor *r, struct cbuffer *src, b_size max_write, error *e)
{
  DBG_ASSERT (rptc_seeked, r);
  ASSERT (r->tx);
  ASSERT (max_write <= NUPD_MAX_DATA_LENGTH);

  err_t_wrap (pgr_maybe_make_writable (r->pager, r->tx, &r->cur, e), e);

  r->inserter = (struct rptc_insert){
    .src = src,
    .total_written = 0,
    .max_write = max_write,
    .output = &r->_nupd1,
    .last = dlgt_get_next (page_h_ro (&r->cur)),
    .tbw = 0,
    .tbl = 0,
  };
  r->inserter.tbl = dl_read_out_from (page_h_w (&r->cur), r->inserter.temp_buf, r->lidx);
  nupd_init (r->inserter.output, page_h_pgno (&r->cur), 0);

  r->state = RPTS_DL_INSERTING;

  DBG_ASSERT (rptc_inserting, r);

  return SUCCESS;
}

/**
 * Advance insert forward one node by creating a new node
 *
 * - Creates a new node to the right (no links set yet)
 * - Appends cur to node updates
 * - Releases cur
 */
static inline err_t
rptc_insert_advance (struct rptree_cursor *r, error *e)
{
  DBG_ASSERT (rptc_inserting, r);

  // Load next
  page_h next = page_h_create ();
  err_t_wrap (pgr_dlgt_new_next_no_link (&r->cur, &next, r->tx, r->pager, e), e);

  i_log_trace ("DL_INSERTING Advancing from page: %" PRpgno " to page: %" PRpgno "\n",
               page_h_pgno (&r->cur),
               page_h_pgno (&next));

  // Release current
  nupd_commit_1st_right (r->inserter.output, pgh_unravel (&r->cur));
  err_t_wrap (pgr_release (r->pager, &r->cur, PG_DATA_LIST, e), e);
  page_h_xfer_ownership_ptr (&r->cur, &next);

  r->lidx = 0;

  return SUCCESS;
}

static inline p_size
rptc_insert_next (struct rptree_cursor *r)
{
  DBG_ASSERT (rptc_inserting, r);

  // Available to write in this node
  p_size next = dl_avail (page_h_ro (&r->cur));

  // Available to write from source
  next = MIN (next, cbuffer_len (r->inserter.src));

  if (r->inserter.max_write > 0)
    {
      next = MIN (next, r->inserter.max_write - r->inserter.total_written);
    }

  return next;
}

err_t
rptc_insert_execute (struct rptree_cursor *r, error *e)
{
  DBG_ASSERT (rptc_inserting, r);

  p_size next = rptc_insert_next (r);

  if (next == 0)
    {
      // Block on backpressure and don't make hasty decisions
      if (cbuffer_len (r->inserter.src) == 0)
        {
          return SUCCESS;
        }

      // Done writing to this page - and there's more data so advance
      else if (r->lidx == DL_DATA_SIZE)
        {
          err_t_wrap (rptc_insert_advance (r, e), e);
          next = rptc_insert_next (r);
          ASSERT (next > 0);
        }

      else
        {
          UNREACHABLE ();
        }
    }

  dl_append_from_cbuffer (page_h_w (&r->cur), r->inserter.src, next);

  // Increment stuff
  r->lidx += next;
  r->total_size += next;
  r->inserter.total_written += next;

  // Totally done
  if (r->inserter.max_write > 0 && r->inserter.max_write == r->inserter.total_written)
    {
      return rptc_insert_to_rebalancing_or_unseeked (r, e);
    }

  return SUCCESS;
}

static err_t
rptc_insert_execute_from_temp_buf (struct rptree_cursor *r, error *e)
{
  DBG_ASSERT (rptc_inserting, r);

  p_size written = dl_append (
      page_h_w (&r->cur),
      r->inserter.temp_buf + r->inserter.tbw,
      r->inserter.tbl - r->inserter.tbw);

  i_log_trace ("DL_INSERTING wrote: %" PRp_size " bytes from temp buf\n", written);

  // Increment stuff
  r->lidx += written;
  r->inserter.tbw += written;

  // Advance pages only if end AND there's more data
  if (r->lidx == DL_DATA_SIZE && r->inserter.tbw < r->inserter.tbl)
    {
      err_t_wrap (rptc_insert_advance (r, e), e);
    }

  return SUCCESS;
}

err_t
rptc_insert_to_rebalancing_or_unseeked (struct rptree_cursor *r, error *e)
{
  DBG_ASSERT (rptc_inserting, r);

  page_h prev = page_h_create ();
  page_h next = page_h_create ();

  // 1. Write out temp buf
  while (r->inserter.tbw < r->inserter.tbl)
    {
      err_t_wrap (rptc_insert_execute_from_temp_buf (r, e), e);
    }

  // 2. Link cur -> last
  if (r->inserter.last != PGNO_NULL && r->inserter.last != dlgt_get_next (page_h_ro (&r->cur)))
    {
      err_t_wrap (pgr_get_writable (&next, r->tx, PG_DATA_LIST, r->inserter.last, r->pager, e), e);
      dlgt_link (page_h_w (&r->cur), page_h_w (&next));
    }

  // 3. Balance cur
  struct three_in_pair output;
  struct root_update root;
  err_t_wrap (rptc_balance_and_release (&output, &root, r, &prev, &next, e), e);
  nupd_append_tip_right (r->inserter.output, output);

  return rptc_rebalance_move_up_stack (r, root, e);
}
