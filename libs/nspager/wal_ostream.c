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
 *   Implements wal_stream.h. Output stream operations for writing WAL records with buffering and flush control.
 */

#include <numstore/core/assert.h>
#include <numstore/core/cbuffer.h>
#include <numstore/core/checksums.h>
#include <numstore/core/latch.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/intf/stdlib.h>
#include <numstore/pager/wal_stream.h>

DEFINE_DBG_ASSERT (
    struct wal_ostream, wal_ostream, w,
    {
      ASSERT (w);
    })

///////////////////////////////////////////////////////
/// LOGR Mode

struct wal_ostream *
walos_open (const char *fname, error *e)
{
  struct wal_ostream *ret = i_malloc (1, sizeof *ret, e);
  if (ret == NULL)
    {
      return NULL;
    }

  if (i_open_w (&ret->fd, fname, e))
    {
      i_free (ret);
      return NULL;
    }

  i64 len = i_seek (&ret->fd, 0, I_SEEK_END, e);

  if (len < 0)
    {
      i_close (&ret->fd, e);
      i_free (ret);
      return NULL;
    }

  err_t err = latch_init (&ret->l, e);
  if (err < SUCCESS)
    {
      i_close (&ret->fd, e);
      i_free (ret);
      return NULL;
    }

  ret->buffer = cbuffer_create (ret->_buffer, sizeof (ret->_buffer));
  ret->flushed_lsn = len;

  DBG_ASSERT (wal_ostream, ret);

  return ret;
}

err_t
walos_close (struct wal_ostream *w, error *e)
{
  DBG_ASSERT (wal_ostream, w);

  walos_flush_to (w, w->flushed_lsn + cbuffer_len (&w->buffer), e);
  i_close (&w->fd, e);
  i_free (w);

  return e->cause_code;
}

///////////////////////////////////////////////////////
/// LOGW Mode

static err_t
walos_flush_to_impl (struct wal_ostream *w, lsn l, bool lock, error *e)
{
  DBG_ASSERT (wal_ostream, w);

  if (lock)
    {
      latch_lock (&w->l);
    }

  ASSERTF (l <= w->flushed_lsn + cbuffer_len (&w->buffer),
           "Trying to flush past a written lsn. Attempt: %" PRlsn " actual last lsn: %" PRlsn "\n",
           l, w->flushed_lsn + cbuffer_len (&w->buffer));

  // i_log_debug ("Flushing the WAL to lsn: %" PRlsn "\n", l);

  if (l > w->flushed_lsn)
    {
      u32 towrite = cbuffer_len (&w->buffer);

      // Flush out the entire file to disk
      if (cbuffer_write_to_file_1_expect (&w->fd, &w->buffer, towrite, e))
        {
          goto theend;
        }
      cbuffer_write_to_file_2 (&w->buffer, towrite);

      w->flushed_lsn += towrite;
    }

  // Need to fsync every flush due to interleaving
  // fsync's
  if (i_fsync (&w->fd, e))
    {
      goto theend;
    }

theend:
  latch_unlock (&w->l);
  return e->cause_code;
}

err_t
walos_flush_to (struct wal_ostream *w, lsn l, error *e)
{
  return walos_flush_to_impl (w, l, true, e);
}

err_t
walos_write_all (struct wal_ostream *w, u32 *checksum, const void *data, u32 len, error *e)
{
  DBG_ASSERT (wal_ostream, w);

  if (checksum)
    {
      checksum_execute (checksum, data, len);
    }

  u32 written = 0;
  const u8 *src = data;

  latch_lock (&w->l);

  while (written < len)
    {
      if (cbuffer_avail (&w->buffer) < (len - written))
        {
          if (walos_flush_to_impl (w, w->flushed_lsn + cbuffer_len (&w->buffer), false, e))
            {
              latch_unlock (&w->l);
              return e->cause_code;
            }
        }

      u32 towrite = MIN (len - written, cbuffer_avail (&w->buffer));
      ASSERT (towrite > 0); // Loop termination condition

      cbuffer_write_expect (src + written, 1, towrite, &w->buffer);

      written += towrite;
    }

  latch_unlock (&w->l);

  return SUCCESS;
}

lsn
walos_get_next_lsn (struct wal_ostream *w)
{
  return w->flushed_lsn + cbuffer_len (&w->buffer);
}

#ifndef NTEST
err_t
walos_crash (struct wal_ostream *w, error *e)
{
  DBG_ASSERT (wal_ostream, w);
  i_close (&w->fd, e);
  i_free (w);
  return e->cause_code;
}
#endif
