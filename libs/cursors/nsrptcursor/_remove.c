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
 *   Implements _remove.h. Provides removal operations for the R+ tree cursor,
 *   handling data deletion with optional output to a destination buffer and
 *   automatic tree rebalancing.
 */

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/intf/types.h>
#include <numstore/pager.h>
#include <numstore/pager/data_list.h>
#include <numstore/pager/page.h>
#include <numstore/pager/page_h.h>
#include <numstore/pager/pager_routines.h>
#include <numstore/rptree/rptree_cursor.h>
#include <numstore/test/page_fixture.h>
#include <numstore/test/testing.h>

static inline page_h *
rptc_remove_src (struct rptree_cursor *r)
{
  if (r->remover.src.mode == PHM_NONE)
    {
      return &r->cur;
    }
  return &r->remover.src;
}

static inline const page_h *
rptc_remove_csrc (const struct rptree_cursor *r)
{
  if (r->remover.src.mode == PHM_NONE)
    {
      return &r->cur;
    }
  return &r->remover.src;
}

DEFINE_DBG_ASSERT (
    struct rptree_cursor, rptc_removing, r,
    {
      ASSERT (r->root != PGNO_NULL);
      ASSERT (r->tx);
      ASSERT (r->remover.sidx <= dl_used (page_h_ro (rptc_remove_csrc (r))));
    })

////////////////////////
/// REMOVE

err_t
rptc_seeked_to_remove (
    struct rptree_cursor *r,
    struct cbuffer *dest,
    b_size max_remove,
    t_size bsize,
    u32 stride,
    error *e)
{
  DBG_ASSERT (rptc_seeked, r);
  ASSERT (bsize > 0);
  ASSERT (stride > 0);
  ASSERT (r->tx);

  err_t_wrap (pgr_maybe_make_writable (r->pager, r->tx, &r->cur, e), e);

  r->remover = (struct rptc_remove){
    .dest = dest,
    .total_removed = 0,
    .bsize = bsize,
    .stride = stride,
    .max_remove = max_remove * bsize,

    .output = &r->_nupd1,
    .src = page_h_create (),
    .sidx = r->lidx,

    .bnext = bsize,
    .state = DLREMOVE_REMOVING,
  };

  nupd_init (r->remover.output, page_h_pgno (&r->cur), dl_used (page_h_ro (&r->cur)));

  r->state = RPTS_DL_REMOVING;

  DBG_ASSERT (rptc_removing, r);

  return SUCCESS;
}

static inline err_t
rptc_remove_advance_cur (struct rptree_cursor *r, error *e)
{
  DBG_ASSERT (rptc_removing, r);
  ASSERT (r->lidx > DL_DATA_SIZE / 2);

  in_set_len (page_h_w (&r->cur), r->lidx);

  nupd_commit_1st_right (r->remover.output, pgh_unravel (&r->cur));

  if (r->remover.src.mode == PHM_NONE)
    {
      err_t_wrap (pgr_dlgt_advance_next (&r->cur, r->tx, r->pager, e), e);
    }
  else
    {
      err_t_wrap (pgr_release (r->pager, &r->cur, PG_DATA_LIST, e), e);
      r->cur = page_h_xfer_ownership (&r->remover.src);
    }

  r->lidx = 0;

  return SUCCESS;
}

/**
 * Advances src forward one node.
 */
static inline err_t
rptc_remove_advance_src (struct rptree_cursor *r, bool *iseof, error *e)
{
  DBG_ASSERT (rptc_removing, r);
  *iseof = false;

  // First time: load next page into src
  if (r->remover.src.mode == PHM_NONE)
    {
      err_t_wrap (pgr_dlgt_get_next (&r->cur, &r->remover.src, r->tx, r->pager, e), e);
    }

  // Rather than unnecessarily deleting cur - just "delete src"
  // by shifting into src
  else if (r->lidx > DL_DATA_SIZE / 2)
    {
      err_t_wrap (rptc_remove_advance_cur (r, e), e);

      // cur == src
      ASSERT (page_h_pgno (&r->cur) == page_h_pgno (rptc_remove_src (r)));

      err_t_wrap (pgr_dlgt_get_next (&r->cur, &r->remover.src, r->tx, r->pager, e), e);
    }

  // Need to fill cur more - delete src and merge into cur
  else
    {
      pgno npg = page_h_pgno (&r->remover.src);
      err_t_wrap (pgr_dlgt_delete_and_fill_next (&r->cur, &r->remover.src, r->tx, r->pager, e), e);
      nupd_append_2nd_right (r->remover.output, pgh_unravel (&r->cur), npg, 0);
    }

  if (r->remover.src.mode == PHM_NONE)
    {
      in_set_len (page_h_w (&r->cur), r->lidx);
      r->remover.sidx = r->lidx;
      *iseof = true;
    }
  else
    {
      r->remover.sidx = 0;
    }

  return SUCCESS;
}

static inline p_size
rptc_remove_removing_next (struct rptree_cursor *r)
{
  // Available in this state
  p_size next = r->remover.bnext;

  // Available to write in this node
  next = MIN (next, DL_DATA_SIZE - r->lidx);

  // Available to write from source node
  next = MIN (next, dl_used (page_h_ro (rptc_remove_src (r))) - r->remover.sidx);

  // Available to write into source
  if (r->remover.dest)
    {
      next = MIN (next, cbuffer_avail (r->remover.dest));
    }

  // How much we can remove forwards
  if (r->remover.max_remove > 0)
    {
      next = MIN (next, r->remover.max_remove - r->remover.total_removed);
    }

  return next;
}

static inline p_size
rptc_remove_skipping_next (struct rptree_cursor *r)
{
  // Available in this state
  p_size next = r->remover.bnext;

  // Available to write from source node
  next = MIN (next, dl_used (page_h_ro (rptc_remove_src (r))) - r->remover.sidx);

  // Cur page is full - TODO - I hastily added this - check it
  next = MIN (next, DL_DATA_SIZE - r->lidx);

  return next;
}

static inline p_size
rptc_remove_drain_src_next (struct rptree_cursor *r)
{
  // Available to write in this node
  p_size next = DL_DATA_SIZE - r->lidx;

  // Available to write from source node
  next = MIN (next, dl_used (page_h_ro (rptc_remove_src (r))) - r->remover.sidx);

  return next;
}

static err_t
rptc_remove_drain_src (struct rptree_cursor *r, error *e)
{
  while (true)
    {
      DBG_ASSERT (rptc_removing, r);

      p_size next = rptc_remove_drain_src_next (r);

      if (next == 0)
        {
          // Reached end of current src page
          if (r->remover.sidx == dl_used (page_h_ro (rptc_remove_src (r))))
            {
              dl_set_used (page_h_w (&r->cur), r->lidx);

              if (r->remover.src.mode != PHM_NONE)
                {
                  pgno npg = page_h_pgno (&r->remover.src);
                  err_t_wrap (pgr_dlgt_delete_and_fill_next (&r->cur, &r->remover.src, r->tx, r->pager, e), e);
                  nupd_append_2nd_right (r->remover.output, pgh_unravel (&r->cur), npg, 0);
                  return SUCCESS;
                }

              return SUCCESS;
            }

          // Cur page is full
          else if (r->lidx >= DL_DATA_SIZE)
            {
              ASSERT (r->remover.src.mode != PHM_NONE);
              err_t_wrap (rptc_remove_advance_cur (r, e), e);
              next = rptc_remove_drain_src_next (r);
              ASSERT (next > 0);
            }
          else
            {
              UNREACHABLE ();
            }
        }

      dl_dl_memmove_permissive (
          page_h_w (&r->cur),
          page_h_ro (rptc_remove_src (r)),
          r->lidx,
          r->remover.sidx,
          next);

      r->lidx += next;
      r->remover.sidx += next;
    }
}

err_t
rptc_remove_execute (struct rptree_cursor *r, error *e)
{
  while (true)
    {
      DBG_ASSERT (rptc_removing, r);

      switch (r->remover.state)
        {
        case DLREMOVE_REMOVING:
          {
            p_size next = rptc_remove_removing_next (r);

            if (next == 0)
              {
                // Block on back pressure - don't make eager decisions here
                if (r->remover.dest && cbuffer_avail (r->remover.dest) == 0)
                  {
                    return SUCCESS;
                  }

                // Reached end of current src page - try to advance forward or quit early
                else if (r->remover.sidx == dl_used (page_h_ro (rptc_remove_src (r))))
                  {
                    bool iseof;
                    err_t_wrap (rptc_remove_advance_src (r, &iseof, e), e);
                    if (iseof)
                      {
                        return rptc_remove_to_rebalancing_or_unseeked (r, e);
                      }
                    else
                      {
                        return SUCCESS;
                      }
                  }

                // Cur page is full
                else if (r->lidx == DL_DATA_SIZE)
                  {
                    return rptc_remove_advance_cur (r, e);
                  }

                UNREACHABLE ();
              }

            ASSERT (next > 0);
            ASSERT (r->remover.total_removed + next <= r->remover.max_remove);

            if (r->remover.dest)
              {
                p_size read = dl_read_into_cbuffer (
                    page_h_ro (rptc_remove_src (r)),
                    r->remover.dest,
                    r->remover.sidx,
                    next);
                ASSERT (read == next);
              }

            r->remover.sidx += next;
            r->remover.total_removed += next;
            ASSERT (r->total_size >= next);
            r->total_size -= next;
            r->remover.bnext -= next;

            if (r->remover.bnext == 0)
              {
                r->remover.bnext = r->remover.bsize * (r->remover.stride - 1);

                if (r->remover.bnext > 0)
                  {
                    r->remover.state = DLREMOVE_SKIPPING;
                  }
                else
                  {
                    // stride == 1, keep removing
                    r->remover.bnext = r->remover.bsize;
                  }
              }

            if (r->remover.max_remove > 0 && r->remover.max_remove == r->remover.total_removed)
              {
                return rptc_remove_to_rebalancing_or_unseeked (r, e);
              }

            break;
          }
        case DLREMOVE_SKIPPING:
          {
            DBG_ASSERT (rptc_removing, r);

            p_size next = rptc_remove_skipping_next (r);

            if (next == 0)
              {
                // Reached end of current src page
                if (r->remover.sidx == dl_used (page_h_ro (rptc_remove_src (r))))
                  {
                    bool iseof;
                    err_t_wrap (rptc_remove_advance_src (r, &iseof, e), e);
                    if (iseof)
                      {
                        return rptc_remove_to_rebalancing_or_unseeked (r, e);
                      }
                    else
                      {
                        return SUCCESS;
                      }
                  }

                // Cur page is full - TODO - I hastily added this - check it
                else if (r->lidx == DL_DATA_SIZE)
                  {
                    return rptc_remove_advance_cur (r, e);
                  }

                UNREACHABLE ();
              }

            ASSERTF (next <= DL_DATA_SIZE - r->lidx,
                     "Next: %d is greater than remaining: %d\n",
                     next, DL_DATA_SIZE - r->lidx);
            ASSERT (next <= dl_used (page_h_ro (rptc_remove_src (r))) - r->remover.sidx);

            dl_dl_memmove_permissive (
                page_h_w (&r->cur),
                page_h_ro (rptc_remove_src (r)),
                r->lidx,
                r->remover.sidx,
                next);

            r->lidx += next;
            r->remover.sidx += next;
            r->remover.bnext -= next;

            if (r->remover.bnext == 0)
              {
                r->remover.bnext = r->remover.bsize;
                r->remover.state = DLREMOVE_REMOVING;
              }

            break;
          }
        }
    }
}

err_t
rptc_remove_to_rebalancing_or_unseeked (struct rptree_cursor *r, error *e)
{
  DBG_ASSERT (rptc_removing, r);

  // Drain out everything in src
  err_t_wrap (rptc_remove_drain_src (r, e), e);

  // Validate complete elements were removed
  if (r->remover.total_removed % r->remover.bsize != 0)
    {
      return error_causef (
          e, ERR_CORRUPT,
          "Removed %" PRb_size " bytes but element size is %" PRt_size
          " - incomplete element indicates corruption",
          r->remover.total_removed, r->remover.bsize);
    }

  // 3. Balance cur
  struct three_in_pair output;
  struct root_update root = { 0 };

  page_h prev = page_h_create ();
  page_h next = page_h_xfer_ownership (&r->remover.src);

  err_t_wrap (rptc_balance_and_release (&output, &root, r, &prev, &next, e), e);
  nupd_append_tip_right (r->remover.output, output);

  // Move up the stack for rebalancing
  return rptc_rebalance_move_up_stack (r, root, e);
}
