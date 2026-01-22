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
 *   Implements wal_stream.h. Input stream operations for reading WAL records sequentially with buffering.
 */

#include <numstore/core/assert.h>
#include <numstore/core/checksums.h>
#include <numstore/core/error.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/intf/stdlib.h>
#include <numstore/pager/wal_stream.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    struct wal_istream, wal_istream, w,
    {
      ASSERT (w);
    })

///////////////////////////////////////////////////////
/// LOGR Mode

err_t
walis_open (struct wal_istream *dest, const char *fname, error *e)
{
  ASSERT (dest);

  err_t_wrap (i_open_rw (&dest->fd, fname, e), e);

  i64 len = i_file_size (&dest->fd, e);
  if (len < 0)
    {
      i_close (&dest->fd, e);
      return e->cause_code;
    }

  if (i_seek (&dest->fd, 0, I_SEEK_SET, e) < 0)
    {
      i_close (&dest->fd, e);
      return e->cause_code;
    }

  dest->curlsn = 0;
  dest->lsnidx = 0;

  err_t_wrap (latch_init (&dest->latch, e), e);

  DBG_ASSERT (wal_istream, dest);

  return SUCCESS;
}

err_t
walis_close (struct wal_istream *w, error *e)
{
  DBG_ASSERT (wal_istream, w);
  return i_close (&w->fd, e);
}

///////////////////////////////////////////////////////
/// LOGW Mode

err_t
walis_seek (struct wal_istream *w, u64 pos, error *e)
{
  latch_lock (&w->latch);

  DBG_ASSERT (wal_istream, w);

  i64 res = i_seek (&w->fd, pos, I_SEEK_SET, e);
  if (res < 0)
    {
      latch_unlock (&w->latch);
      return e->cause_code;
    }

  if ((u64)res != pos)
    {
      latch_unlock (&w->latch);
      return error_causef (e, ERR_CORRUPT, "Tried to seek to an invalid spot");
    }

  w->curlsn = pos;

  latch_unlock (&w->latch);

  return SUCCESS;
}

void
walis_mark_start_log (struct wal_istream *w)
{
  latch_lock (&w->latch);
  w->lsnidx = 0;
  latch_unlock (&w->latch);
}

err_t
walis_read_all (struct wal_istream *w, bool *iseof, lsn *rlsn, u32 *checksum, void *data, u32 len, error *e)
{
  latch_lock (&w->latch);

  DBG_ASSERT (wal_istream, w);

  *iseof = false;
  if (rlsn)
    {
      *rlsn = w->curlsn;
    }

  i64 bread = i_read_all (&w->fd, data, len, e);
  if (bread < 0)
    {
      latch_unlock (&w->latch);
      return e->cause_code;
    }

  if (bread < len)
    {
      // Hit EOF - incomplete record
      // Truncate to where this record started (before any bytes were copied)
      if (bread > 0)
        {
          if (i_truncate (&w->fd, w->curlsn, e))
            {
              latch_unlock (&w->latch);
              return e->cause_code;
            }
          if (i_fsync (&w->fd, e))
            {
              latch_unlock (&w->latch);
              return e->cause_code;
            }
        }
      *iseof = true;
      latch_unlock (&w->latch);
      return SUCCESS;
    }

  if (checksum)
    {
      checksum_execute (checksum, data, len);
    }

  w->lsnidx += len;

  latch_unlock (&w->latch);

  return SUCCESS;
}

void
walis_mark_end_log (struct wal_istream *w)
{
  latch_lock (&w->latch);

  w->curlsn += w->lsnidx;

  latch_unlock (&w->latch);
}

#ifndef NTEST
err_t
walis_crash (struct wal_istream *w, error *e)
{
  DBG_ASSERT (wal_istream, w);
  return i_close (&w->fd, e);
}
#endif
