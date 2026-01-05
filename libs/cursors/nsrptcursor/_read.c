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
 *   Implements _read.h. Provides sequential read operations for the R+ tree cursor,
 *   supporting strided reads and block size specifications.
 */

#include <numstore/core/error.h>
#include <numstore/pager.h>
#include <numstore/pager/data_list.h>
#include <numstore/pager/page.h>
#include <numstore/pager/page_h.h>
#include <numstore/pager/pager_routines.h>
#include <numstore/rptree/rptree_cursor.h>
#include <numstore/test/page_fixture.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    struct rptree_cursor, rptc_reading, r,
    {
      ASSERT (r->root != PGNO_NULL);
      ASSERT (r->reader.bnext > 0);
      ASSERT (r->reader.max_bread == 0 || r->reader.total_bread <= r->reader.max_bread);
    })

////////////////////////
/// READ

void
rptc_seeked_to_read (
    struct rptree_cursor *r,
    struct cbuffer *dest,
    b_size max_nread,
    t_size bsize,
    u32 stride)
{
  DBG_ASSERT (rptc_seeked, r);

  ASSERT (bsize > 0);
  ASSERT (stride > 0);

  r->reader = (struct rptc_read){
    .dest = dest,
    .bsize = bsize,
    .stride = stride,
    .bnext = bsize,
    .total_bread = 0,
    .max_bread = max_nread * bsize,
    .state = DLREAD_ACTIVE,
  };

  r->state = RPTS_DL_READING;

  DBG_ASSERT (rptc_reading, r);
}

static inline p_size
rptc_read_next (struct rptree_cursor *r)
{
  DBG_ASSERT (rptc_reading, r);

  // Available in the buffer
  p_size next = dl_used (page_h_ro (&r->cur)) - r->lidx;

  // Available in this read state
  next = MIN (next, r->reader.bnext);

  // Available to write to (only when actively reading)
  if (r->reader.dest && r->reader.state == DLREAD_ACTIVE)
    {
      next = MIN (next, cbuffer_avail (r->reader.dest));
    }

  // Available globally to read
  if (r->reader.max_bread > 0 && r->reader.state == DLREAD_ACTIVE)
    {
      next = MIN (next, r->reader.max_bread - r->reader.total_bread);
    }

  return next;
}

err_t
rptc_read_execute (struct rptree_cursor *r, error *e)
{
  DBG_ASSERT (rptc_reading, r);
  const page *cur = page_h_ro (&r->cur);

  while (true)
    {
      // Check if we've completed the read operation
      if (r->reader.max_bread > 0 && r->reader.total_bread >= r->reader.max_bread)
        {
          return rptc_read_to_unseeked (r, e);
        }

      p_size next = rptc_read_next (r);

      if (next == 0)
        {
          // Block on dest backpressure
          if (r->reader.dest && cbuffer_avail (r->reader.dest) == 0)
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
                  return rptc_read_to_unseeked (r, e);
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

      switch (r->reader.state)
        {
        case DLREAD_ACTIVE:
          {
            if (next > 0)
              {
                if (r->reader.dest)
                  {
                    p_size read = dl_read_into_cbuffer (cur, r->reader.dest, r->lidx, next);
                    ASSERT (read == next);
                  }
                r->lidx += next;
                r->reader.total_bread += next;
                r->reader.bnext -= next;
              }

            if (r->reader.bnext == 0)
              {
                r->reader.bnext = (r->reader.stride - 1) * r->reader.bsize;
                r->reader.state = DLREAD_SKIPPING;

                // TODO - Optimize stride = 1
                if (r->reader.bnext == 0)
                  {
                    r->reader.bnext = r->reader.bsize;
                    r->reader.state = DLREAD_ACTIVE;
                  }
              }
            break;
          }

        case DLREAD_SKIPPING:
          {
            if (next > 0)
              {
                p_size read = dl_read (cur, NULL, r->lidx, next);
                ASSERT (read == next);
                r->lidx += next;
                r->reader.bnext -= next;
              }

            if (r->reader.bnext == 0)
              {
                r->reader.bnext = r->reader.bsize;
                r->reader.state = DLREAD_ACTIVE;
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
rptc_read_to_unseeked (struct rptree_cursor *r, error *e)
{
  DBG_ASSERT (rptc_reading, r);
  ASSERT (r->state == RPTS_DL_READING);

  // Release current page
  err_t_wrap (pgr_release (r->pager, &r->cur, PG_DATA_LIST, e), e);

  // Pop stack and reset to unseeked state
  r->state = RPTS_PERMISSIVE;
  err_t_wrap (rptc_pop_all (r, e), e);

  r->state = RPTS_UNSEEKED;
  r->lidx = 0;

  // Validate we read complete elements
  if (r->reader.total_bread % r->reader.bsize != 0)
    {
      return error_causef (
          e, ERR_CORRUPT,
          "Read %" PRb_size " bytes but element size is %" PRt_size
          " - incomplete element indicates corruption",
          r->reader.total_bread, r->reader.bsize);
    }

  return SUCCESS;
}
