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
 *   Implements _write.h. Provides write operations to overwrite existing data
 *   in the R+ tree at the current cursor position, supporting strided writes
 *   and block specifications.
 */

#include "numstore/pager/data_list.h"
#include <numstore/pager/pager_routines.h>
#include <numstore/rptree/rptree_cursor.h>

DEFINE_DBG_ASSERT (
    struct rptree_cursor, rptc_writing, r,
    {
      ASSERT (r->root != PGNO_NULL);
      ASSERT (r->writer.stride >= 1);
      ASSERT (r->tx);
      ASSERT (r->writer.max_bwrite == 0 || r->writer.total_bwritten <= r->writer.max_bwrite);
    })

////////////////////////
/// WRITE

err_t
rptc_seeked_to_write (
    struct rptree_cursor *r,
    struct cbuffer *src,
    b_size max_nwrite,
    t_size bsize,
    u32 stride,
    error *e)
{
  ASSERT (r->state == RPTS_SEEKED);
  ASSERT (src);
  ASSERT (r->tx);
  ASSERT (bsize > 0);
  ASSERT (stride > 0);

  err_t_wrap (pgr_maybe_make_writable (r->pager, r->tx, &r->cur, e), e);

  r->writer = (struct rptc_write){
    .src = src,
    .bsize = bsize,
    .stride = stride,
    .bstrlen = bsize * stride,
    .bnext = bsize,
    .total_bwritten = 0,
    .max_bwrite = max_nwrite * bsize,
    .state = DLWRITE_ACTIVE,
  };

  r->state = RPTS_DL_WRITING;

  DBG_ASSERT (rptc_writing, r);

  return SUCCESS;
}

static inline p_size
rptc_write_next (struct rptree_cursor *r)
{
  DBG_ASSERT (rptc_writing, r);

  // Available in the buffer
  p_size next = dl_used (page_h_ro (&r->cur)) - r->lidx;

  // Available in this write state
  next = MIN (next, r->writer.bnext);

  // Available to write to (only when actively writeing)
  if (r->writer.state == DLWRITE_ACTIVE)
    {
      next = MIN (next, cbuffer_len (r->writer.src));
    }

  // Available globally to write
  if (r->writer.max_bwrite > 0 && r->writer.state == DLWRITE_ACTIVE)
    {
      next = MIN (next, r->writer.max_bwrite - r->writer.total_bwritten);
    }

  return next;
}

err_t
rptc_write_execute (struct rptree_cursor *r, error *e)
{
  DBG_ASSERT (rptc_writing, r);
  page *cur = page_h_w (&r->cur);
  while (true)
    {
      // Check if we've completed the write operation
      if (r->writer.max_bwrite > 0 && r->writer.total_bwritten >= r->writer.max_bwrite)
        {
          return rptc_write_to_unseeked (r, e);
        }
      p_size next = rptc_write_next (r);
      if (next == 0)
        {
          // Block on source backpressure
          if (r->writer.src && cbuffer_len (r->writer.src) == 0)
            {
              return SUCCESS;
            }
          // Reached end of current page, advance to next
          else if (r->lidx >= dl_used (cur))
            {
              page_h next_page = page_h_create ();
              err_t_wrap (pgr_dlgt_get_next (&r->cur, &next_page, r->tx, r->pager, e), e);
              // Reached EOF
              if (next_page.mode == PHM_NONE)
                {
                  return SUCCESS;
                }
              else
                {
                  err_t_wrap (pgr_release (r->pager, &r->cur, PG_DATA_LIST, e), e);
                  r->lidx = 0;
                  r->cur = page_h_xfer_ownership (&next_page);
                  return SUCCESS;
                }
            }
          else
            {
              UNREACHABLE ();
            }
        }
      switch (r->writer.state)
        {
        case DLWRITE_ACTIVE:
          {
            if (next > 0)
              {
                if (r->writer.src)
                  {
                    p_size written = dl_write_from_buffer (cur, r->writer.src, r->lidx, next);
                    ASSERT (written == next);
                  }
                r->lidx += next;
                r->writer.total_bwritten += next;
                r->writer.bnext -= next;
              }
            if (r->writer.bnext == 0)
              {
                r->writer.bnext = (r->writer.stride - 1) * r->writer.bsize;
                r->writer.state = DLWRITE_SKIPPING;
                // TODO - Optimize stride = 1
                if (r->writer.bnext == 0)
                  {
                    r->writer.bnext = r->writer.bsize;
                    r->writer.state = DLWRITE_ACTIVE;
                  }
              }
            break;
          }
        case DLWRITE_SKIPPING:
          {
            if (next > 0)
              {
                r->lidx += next;
                r->writer.bnext -= next;
              }
            if (r->writer.bnext == 0)
              {
                r->writer.bnext = r->writer.bsize;
                r->writer.state = DLWRITE_ACTIVE;
              }
            break;
          }
        default:
          {
            UNREACHABLE ();
          }
        }
    }
}

err_t
rptc_write_to_unseeked (struct rptree_cursor *r, error *e)
{
  DBG_ASSERT (rptc_writing, r);
  ASSERT (r->state == RPTS_DL_WRITING);

  // release(cur)
  err_t_wrap (pgr_release (r->pager, &r->cur, PG_DATA_LIST, e), e);

  r->state = RPTS_PERMISSIVE;

  // pop_all(stack)
  err_t_wrap (rptc_pop_all (r, e), e);

  r->state = RPTS_UNSEEKED;
  r->lidx = 0;

  return SUCCESS;
}
