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

#include <numstore/pager/pager_routines.h>
#include <numstore/rptree/rptree_cursor.h>

DEFINE_DBG_ASSERT (
    struct rptree_cursor, rptc_writing, r,
    {
      ASSERT (r->root != PGNO_NULL);
      ASSERT (r->writer.stride >= 1);
      ASSERT (r->tx);
    })

////////////////////////
/// WRITE

void
rptc_seeked_to_write (struct rptree_cursor *r, struct cbuffer *src, t_size bsize, u32 stride)
{
  ASSERT (r->state == RPTS_SEEKED);
  ASSERT (src);
  ASSERT (bsize > 0);
  ASSERT (stride > 0);

  r->writer.src = src;
  r->writer.bsize = bsize;
  r->writer.stride = stride;
  r->writer.bstrlen = bsize * stride;
  r->writer.bnext = bsize;
  r->writer.total_written = 0;
  r->writer.state = DLWRITE_ACTIVE;

  r->state = RPTS_DL_WRITING;

  DBG_ASSERT (rptc_writing, r);
}

err_t
rptc_write_execute (struct rptree_cursor *r, error *e)
{
  DBG_ASSERT (rptc_writing, r);
  ASSERT (r->state == RPTS_DL_WRITING);

  page *cur = page_h_w (&r->cur);

  p_size limit = dl_used (cur);

  while (r->lidx < limit)
    {
      // Get the next total we should write on
      p_size page_avail = limit - r->lidx; // Available in this page
      p_size next = MIN (page_avail, r->writer.bnext);

      switch (r->writer.state)
        {
        case DLWRITE_ACTIVE:
          {
            p_size written = dl_write_from_buffer (cur, r->writer.src, r->lidx, next);

            r->lidx += written;
            r->writer.total_written += written;
            r->writer.bnext -= written;

            if (r->writer.bnext == 0)
              {
                r->writer.bnext = (r->writer.stride - 1) * r->writer.bsize;
                r->writer.state = DLWRITE_SKIPPING;
              }

            break;
          }
        case DLWRITE_SKIPPING:
          {
            p_size written = dl_write (cur, NULL, r->lidx, next);

            r->lidx += written;
            r->writer.bnext -= written;

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

  // EOF
  bool done_this_page = r->lidx == dl_used (page_h_ro (&r->cur));
  bool last_page = dlgt_get_next (page_h_ro (&r->cur)) == PGNO_NULL;
  if (done_this_page && last_page)
    {
      if (r->writer.total_written % r->writer.bsize != 0)
        {
          return error_causef (
              e, ERR_CORRUPT,
              "Wrote out %" PRb_size " bytes and reached the end of the file but data "
              "elements are of size: %" PRt_size " which means there's some corruption",
              r->writer.total_written, r->writer.bsize);
        }
      return rptc_write_to_unseeked (r, e);
    }

  err_t_wrap (pgr_dlgt_advance_next (&r->cur, r->tx, r->pager, e), e);

  return SUCCESS;
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
