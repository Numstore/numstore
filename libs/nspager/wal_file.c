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
 *   Implements wal_file.h. WAL file I/O operations including log writing, reading, and file management.
 */

#include <numstore/pager/wal_file.h>

#include <numstore/core/assert.h>
#include <numstore/core/checksums.h>
#include <numstore/core/error.h>
#include <numstore/core/latch.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/pager/dirty_page_table.h>
#include <numstore/pager/page.h>
#include <numstore/pager/txn_table.h>
#include <numstore/pager/wal_stream.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    struct wal_file, wal_file, w, {
      ASSERT (w);
    })

static inline err_t
walf_lazy_ostream_init (struct wal_file *w, error *e)
{
  if (w->current_ostream == NULL)
    {
      w->current_ostream = walos_open (w->fname, e);
      if (w->current_ostream == NULL)
        {
          return e->cause_code;
        }
    }
  return SUCCESS;
}

static inline err_t
walf_lazy_istream_init (struct wal_file *w, error *e)
{
  if (!w->istream_open)
    {
      err_t_wrap (walis_open (&w->istream, w->fname, e), e);
      w->istream_open = true;
    }
  return SUCCESS;
}

static inline err_t
walf_lazy_ostream_close (struct wal_file *w, error *e)
{
  if (w->current_ostream != NULL)
    {
      err_t_wrap (walos_close (w->current_ostream, e), e);
      w->current_ostream = NULL;
    }
  return SUCCESS;
}

static inline err_t
walf_lazy_istream_close (struct wal_file *w, error *e)
{
  if (w->istream_open)
    {
      err_t_wrap (walis_close (&w->istream, e), e);
      w->istream_open = false;
    }
  return SUCCESS;
}

/////////////////////////////////////////////
/// LIFECYCLE

err_t
walf_open (struct wal_file *dest, const char *fname, error *e)
{
  dest->current_ostream = NULL;
  dest->istream_open = false;
  dest->fname = fname;

  latch_init (&dest->l);

  DBG_ASSERT (wal_file, dest);

  return SUCCESS;
}

err_t
walf_reset (struct wal_file *dest, error *e)
{
  err_t_wrap (walf_close (dest, e), e);
  err_t_wrap (i_remove_quiet (dest->fname, e), e);
  err_t_wrap (walf_open (dest, dest->fname, e), e);
  return SUCCESS;
}

err_t
walf_close (struct wal_file *w, error *e)
{
  DBG_ASSERT (wal_file, w);

  err_t_wrap (walf_lazy_istream_close (w, e), e);
  err_t_wrap (walf_lazy_ostream_close (w, e), e);

  return e->cause_code;
}

err_t
walf_write_mode (struct wal_file *w, error *e)
{
  DBG_ASSERT (wal_file, w);

  return walf_lazy_istream_close (w, e);
}

/////////////////////////////////////////////
/// WRITE

static inline err_t
walf_write_update (struct wal_file *w, const struct wal_rec_hdr_write *r, error *e)
{
  DBG_ASSERT (wal_file, w);

  latch_lock (&w->l);

  err_t_wrap_goto (walf_lazy_ostream_init (w, e), theend, e);

  u32 checksum = checksum_init ();
  wlh t = (wlh)r->type;
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &t, sizeof (wlh), e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &r->update.tid, sizeof (txid), e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &r->update.prev, sizeof (lsn), e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &r->update.pg, sizeof (pgno), e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, r->update.undo, PAGE_SIZE, e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, r->update.redo, PAGE_SIZE, e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, NULL, &checksum, sizeof (u32), e), theend, e);

theend:
  latch_unlock (&w->l);
  return e->cause_code;
}

static inline err_t
walf_write_clr (struct wal_file *w, const struct wal_rec_hdr_write *r, error *e)
{
  DBG_ASSERT (wal_file, w);
  ASSERT (r->type == WL_CLR);
  ASSERT (r->clr.undo_next != 0);

  latch_lock (&w->l);

  err_t_wrap_goto (walf_lazy_ostream_init (w, e), theend, e);

  u32 checksum = checksum_init ();
  wlh t = r->type;
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &t, sizeof (wlh), e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &r->clr.tid, sizeof (txid), e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &r->clr.prev, sizeof (lsn), e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &r->clr.pg, sizeof (pgno), e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &r->clr.undo_next, sizeof (lsn), e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, r->clr.redo, PAGE_SIZE, e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, NULL, &checksum, sizeof (u32), e), theend, e);

theend:
  latch_unlock (&w->l);
  return e->cause_code;
}

static inline err_t
walf_write_begin (struct wal_file *w, const struct wal_rec_hdr_write *r, error *e)
{
  DBG_ASSERT (wal_file, w);
  ASSERT (r->type == WL_BEGIN);

  latch_lock (&w->l);

  err_t_wrap_goto (walf_lazy_ostream_init (w, e), theend, e);

  u32 checksum = checksum_init ();
  wlh t = r->type;
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &t, sizeof (wlh), e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &r->begin.tid, sizeof (txid), e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, NULL, &checksum, sizeof (u32), e), theend, e);

theend:
  latch_unlock (&w->l);
  return e->cause_code;
}

static inline err_t
walf_write_commit (struct wal_file *w, const struct wal_rec_hdr_write *r, error *e)
{
  DBG_ASSERT (wal_file, w);
  ASSERT (r->type == WL_COMMIT);

  latch_lock (&w->l);

  err_t_wrap_goto (walf_lazy_ostream_init (w, e), theend, e);

  u32 checksum = checksum_init ();
  wlh t = r->type;
  txid tid = r->commit.tid;
  lsn prev = r->commit.prev;

  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &t, sizeof (wlh), e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &tid, sizeof (txid), e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &prev, sizeof (lsn), e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, NULL, &checksum, sizeof (u32), e), theend, e);

theend:
  latch_unlock (&w->l);
  return e->cause_code;
}

static inline err_t
walf_write_end (struct wal_file *w, const struct wal_rec_hdr_write *r, error *e)
{
  DBG_ASSERT (wal_file, w);
  ASSERT (r->type == WL_END);

  latch_lock (&w->l);

  err_t_wrap_goto (walf_lazy_ostream_init (w, e), theend, e);

  u32 checksum = checksum_init ();
  wlh t = r->type;
  txid tid = r->end.tid;
  lsn prev = r->end.prev;

  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &t, sizeof (wlh), e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &tid, sizeof (txid), e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &prev, sizeof (lsn), e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, NULL, &checksum, sizeof (u32), e), theend, e);

theend:
  latch_unlock (&w->l);
  return e->cause_code;
}

static inline err_t
walf_write_ckpt_begin (struct wal_file *w, const struct wal_rec_hdr_write *r, error *e)
{
  DBG_ASSERT (wal_file, w);
  ASSERT (r->type == WL_CKPT_BEGIN);

  latch_lock (&w->l);

  err_t_wrap_goto (walf_lazy_ostream_init (w, e), theend, e);

  u32 checksum = checksum_init ();
  wlh t = r->type;
  err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &t, sizeof (wlh), e), theend, e);
  err_t_wrap_goto (walos_write_all (w->current_ostream, NULL, &checksum, sizeof (u32), e), theend, e);

theend:
  latch_unlock (&w->l);
  return e->cause_code;
}

static inline err_t
walf_write_ckpt_end (struct wal_file *w, const struct wal_rec_hdr_write *r, error *e)
{
  DBG_ASSERT (wal_file, w);
  ASSERT (r->type == WL_CKPT_END);

  void *att_serialized = NULL;
  u8 dpgt_serialized[MAX_DPGT_SRL_SIZE];
  u32 attsize = 0;
  u32 dptsize = 0;

  // Capture ATT / DPT state and serialize
  {
    latch_lock (&r->ckpt_end.att->l);
    latch_lock (&r->ckpt_end.dpt->l);

    // ATT
    attsize = txnt_get_serialize_size (r->ckpt_end.att);
    if (attsize > 0)
      {
        att_serialized = i_malloc (attsize, 1, e);
        if (att_serialized == NULL)
          {
            latch_unlock (&r->ckpt_end.dpt->l);
            latch_unlock (&r->ckpt_end.att->l);
            return e->cause_code;
          }
        txnt_serialize (att_serialized, attsize, r->ckpt_end.att);
      }

    // DPT
    dptsize = dpgt_serialize (dpgt_serialized, r->ckpt_end.dpt);

    latch_unlock (&r->ckpt_end.dpt->l);
    latch_unlock (&r->ckpt_end.att->l);
  }

  // Write WAL
  {
    latch_lock (&w->l);
    err_t_wrap_goto (walf_lazy_ostream_init (w, e), cleanup, e);

    u32 checksum = checksum_init ();
    wlh t = r->type;

    err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &t, sizeof (wlh), e), cleanup, e);
    err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &attsize, sizeof (attsize), e), cleanup, e);
    err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, &dptsize, sizeof (dptsize), e), cleanup, e);

    // Transaction table
    if (attsize > 0)
      {
        err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, att_serialized, attsize, e), cleanup, e);
      }

    // Dirty page table
    if (dptsize > 0)
      {
        err_t_wrap_goto (walos_write_all (w->current_ostream, &checksum, dpgt_serialized, dptsize, e), cleanup, e);
      }

    err_t_wrap_goto (walos_write_all (w->current_ostream, NULL, &checksum, sizeof (u32), e), cleanup, e);

  cleanup:
    latch_unlock (&w->l);
    if (att_serialized)
      {
        i_free (att_serialized);
      }
    return e->cause_code;
  }
}

slsn
walf_write (struct wal_file *w, const struct wal_rec_hdr_write *r, error *e)
{
  /**
  DBG_ASSERT (wal_file, w);
  err_t_wrap (walf_lazy_ostream_init (w, e), e);

  lsn ret = walos_get_next_lsn (w->current_ostream);

  switch (r->type)
    {
    case WL_BEGIN:
      {
        err_t_wrap (walf_write_begin (w, r, e), e);
        break;
      }

    case WL_COMMIT:
      {
        err_t_wrap (walf_write_commit (w, r, e), e);
        break;
      }

    case WL_END:
      {
        err_t_wrap (walf_write_end (w, r, e), e);
        break;
      }

    case WL_UPDATE:
      {
        err_t_wrap (walf_write_update (w, r, e), e);
        break;
      }

    case WL_CLR:
      {
        err_t_wrap (walf_write_clr (w, r, e), e);
        break;
      }

    case WL_CKPT_BEGIN:
      {
        err_t_wrap (walf_write_ckpt_begin (w, r, e), e);
        break;
      }

    case WL_CKPT_END:
      {
        err_t_wrap (walf_write_ckpt_end (w, r, e), e);
        break;
      }
    case WL_EOF:
      {
        UNREACHABLE ();
      }
    }

  return ret;
*/
  return SUCCESS;
}

/////////////////////////////////////////////
/// READ FULL

static inline int
walf_read_ckpt_end_full (
    struct wal_file *w,
    u32 *checksum,
    void **buf,
    error *e)
{
  DBG_ASSERT (wal_file, w);
  ASSERT (buf);

  *buf = NULL;
  void *_buf = NULL;
  wlh type = WL_CKPT_END;
  u32 attsize;
  u32 dptsize;

  // att and dpt
  {
    bool iseof;
    u32 sizes[2];
    if (walis_read_all (&w->istream, &iseof, NULL, checksum, sizes, sizeof (sizes), e))
      {
        return e->cause_code;
      }
    if (iseof)
      {
        return WL_EOF;
      }

    attsize = sizes[0];
    dptsize = sizes[1];
  }

  u32 size = sizeof (type)       // Header
             + 2 * sizeof (u32)  // sizes
             + attsize + dptsize // data
             + sizeof (u32);     // checksum

  ///////////////////////////////////////////////////////////////////////////
  /// MALLOC
  _buf = i_malloc (size, 1, e);
  if (_buf == NULL)
    {
      return e->cause_code;
    }

  u8 *head = _buf;

  // Copy type attsize and dptsize over
  {
    i_memcpy (head, &type, sizeof (wlh));
    head += sizeof (wlh);
    i_memcpy (head, &attsize, sizeof (u32));
    head += sizeof (u32);
    i_memcpy (head, &dptsize, sizeof (u32));
    head += sizeof (u32);
  }

  // Everything else
  {
    if (attsize + dptsize > 0)
      {
        bool iseof;
        if (walis_read_all (&w->istream, &iseof, NULL, checksum, head, attsize + dptsize, e))
          {
            i_free (_buf);
            return e->cause_code;
          }
        if (iseof)
          {
            i_free (_buf);
            return WL_EOF;
          }
      }
    head += attsize + dptsize;

    // Checksum
    bool iseof;
    if (walis_read_all (&w->istream, &iseof, NULL, NULL, head, sizeof (u32), e))
      {
        i_free (_buf);
        return e->cause_code;
      }
    if (iseof)
      {
        i_free (_buf);
        return WL_EOF;
      }
  }

  u32 actual_crc;
  i_memcpy (&actual_crc, head, sizeof (u32));

  if (*checksum != actual_crc)
    {
      i_free (_buf);
      return error_causef (e, ERR_CORRUPT, "Invalid CRC");
    }

  *buf = _buf;

  return SUCCESS;
}

static inline int
walf_read_full (struct wal_file *w, u32 *checksum, wlh type, u8 *buf, u32 total_len, error *e)
{
  DBG_ASSERT (wal_file, w);
  ASSERT (total_len >= sizeof (wlh) + sizeof (u32));

  i_memcpy (buf, &type, sizeof (wlh));

  // Do read
  {
    // Everything else
    u8 *head = buf + sizeof (wlh);
    u32 toread = total_len - sizeof (wlh) - sizeof (u32);
    if (toread > 0)
      {
        bool iseof;
        err_t_wrap (walis_read_all (&w->istream, &iseof, NULL, checksum, head, toread, e), e);
        if (iseof)
          {
            return WL_EOF;
          }
      }

    // Checksum
    head += toread;
    toread = sizeof (u32);

    bool iseof;
    err_t_wrap (walis_read_all (&w->istream, &iseof, NULL, NULL, head, toread, e), e);
    if (iseof)
      {
        return WL_EOF;
      }
  }

  u32 actual_crc;
  i_memcpy (&actual_crc, buf + total_len - sizeof (u32), sizeof (u32));

  if (*checksum != actual_crc)
    {
      return error_causef (e, ERR_CORRUPT, "Invalid CRC");
    }

  return SUCCESS;
}

/////////////////////////////////////////////
/// PREAD

static inline err_t
walf_read_update (struct wal_file *w, u32 *checksum, struct wal_rec_hdr_read *r, error *e)
{
  DBG_ASSERT (wal_file, w);
  ASSERT (r->type == WL_UPDATE);

  u8 buf[WL_UPDATE_LEN];
  int ret = walf_read_full (w, checksum, r->type, buf, WL_UPDATE_LEN, e);
  err_t_wrap (ret, e);
  if (ret == WL_EOF)
    {
      r->type = WL_EOF;
      return SUCCESS;
    }
  else
    {
      ASSERT (ret == SUCCESS);
    }

  walf_decode_update (r, buf);

  return SUCCESS;
}

static inline err_t
walf_read_clr (struct wal_file *w, u32 *checksum, struct wal_rec_hdr_read *r, error *e)
{
  DBG_ASSERT (wal_file, w);
  ASSERT (r->type == WL_CLR);

  u8 buf[WL_CLR_LEN];
  int ret = walf_read_full (w, checksum, r->type, buf, WL_CLR_LEN, e);
  err_t_wrap (ret, e);
  if (ret == WL_EOF)
    {
      r->type = WL_EOF;
      return SUCCESS;
    }
  else
    {
      ASSERT (ret == SUCCESS);
    }

  walf_decode_clr (r, buf);

  return SUCCESS;
}

static inline err_t
walf_read_begin (struct wal_file *w, u32 *checksum, struct wal_rec_hdr_read *r, error *e)
{
  DBG_ASSERT (wal_file, w);
  ASSERT (r->type == WL_BEGIN);

  u8 buf[WL_BEGIN_LEN];
  int ret = walf_read_full (w, checksum, r->type, buf, WL_BEGIN_LEN, e);
  err_t_wrap (ret, e);
  if (ret == WL_EOF)
    {
      r->type = WL_EOF;
      return SUCCESS;
    }
  else
    {
      ASSERT (ret == SUCCESS);
    }

  walf_decode_begin (r, buf);

  return SUCCESS;
}

static inline err_t
walf_read_commit (struct wal_file *w, u32 *checksum, struct wal_rec_hdr_read *r, error *e)
{
  DBG_ASSERT (wal_file, w);
  ASSERT (r->type == WL_COMMIT);

  u8 buf[WL_COMMIT_LEN];
  int ret = walf_read_full (w, checksum, r->type, buf, WL_COMMIT_LEN, e);
  err_t_wrap (ret, e);
  if (ret == WL_EOF)
    {
      r->type = WL_EOF;
      return SUCCESS;
    }
  else
    {
      ASSERT (ret == SUCCESS);
    }

  walf_decode_commit (r, buf);

  return SUCCESS;
}

static inline err_t
walf_read_end (struct wal_file *w, u32 *checksum, struct wal_rec_hdr_read *r, error *e)
{
  DBG_ASSERT (wal_file, w);
  ASSERT (r->type == WL_END);

  u8 buf[WL_END_LEN];
  int ret = walf_read_full (w, checksum, r->type, buf, WL_END_LEN, e);
  err_t_wrap (ret, e);
  if (ret == WL_EOF)
    {
      r->type = WL_EOF;
      return SUCCESS;
    }
  else
    {
      ASSERT (ret == SUCCESS);
    }

  walf_decode_end (r, buf);

  return SUCCESS;
}

static inline err_t
walf_read_ckpt_begin (struct wal_file *w, u32 *checksum, struct wal_rec_hdr_read *r, error *e)
{
  DBG_ASSERT (wal_file, w);
  ASSERT (r->type == WL_CKPT_BEGIN);

  u8 buf[WL_CKPT_BEGIN_LEN];
  int ret = walf_read_full (w, checksum, r->type, buf, WL_CKPT_BEGIN_LEN, e);
  err_t_wrap (ret, e);
  if (ret == WL_EOF)
    {
      r->type = WL_EOF;
      return SUCCESS;
    }
  else
    {
      ASSERT (ret == SUCCESS);
    }

  // walf_decode_begin - no need

  return SUCCESS;
}

static inline err_t
walf_read_ckpt_end (struct wal_file *w, u32 *checksum, struct wal_rec_hdr_read *r, error *e)
{
  DBG_ASSERT (wal_file, w);
  ASSERT (r->type == WL_CKPT_BEGIN || r->type == WL_CKPT_END);

  void *buf;
  int ret = walf_read_ckpt_end_full (w, checksum, &buf, e);
  err_t_wrap (ret, e);

  if (ret == WL_EOF)
    {
      r->type = WL_EOF;
      ASSERT (buf == NULL);
      return SUCCESS;
    }
  else
    {
      ASSERT (ret == SUCCESS);
      ASSERT (buf);
    }

  if (walf_decode_ckpt_end (r, buf, e))
    {
      i_free (buf);
      return e->cause_code;
    }

  i_free (buf);

  return SUCCESS;
}

err_t
walf_pread (struct wal_rec_hdr_read *dest, struct wal_file *w, lsn ofst, error *e)
{
  DBG_ASSERT (wal_file, w);
  err_t_wrap (walf_lazy_istream_init (w, e), e);

  wlh t;
  u32 checksum = checksum_init ();
  lsn rlsn;
  err_t_wrap (walis_seek (&w->istream, ofst, e), e);

  // Do read or terminate early
  {
    walis_mark_start_log (&w->istream);
    bool iseof;
    err_t_wrap (walis_read_all (&w->istream, &iseof, &rlsn, &checksum, &t, sizeof (t), e), e);
    if (iseof)
      {
        dest->type = WL_EOF;
        return SUCCESS;
      }
  }

  switch (t)
    {
    case WL_UPDATE:
      {
        dest->type = t;
        err_t_wrap (walf_read_update (w, &checksum, dest, e), e);
        break;
      }
    case WL_CLR:
      {
        dest->type = t;
        err_t_wrap (walf_read_clr (w, &checksum, dest, e), e);
        break;
      }
    case WL_BEGIN:
      {
        dest->type = t;
        err_t_wrap (walf_read_begin (w, &checksum, dest, e), e);
        break;
      }
    case WL_COMMIT:
      {
        dest->type = t;
        err_t_wrap (walf_read_commit (w, &checksum, dest, e), e);
        break;
      }
    case WL_END:
      {
        dest->type = t;
        err_t_wrap (walf_read_end (w, &checksum, dest, e), e);
        break;
      }
    case WL_CKPT_BEGIN:
      {
        dest->type = t;
        err_t_wrap (walf_read_ckpt_begin (w, &checksum, dest, e), e);
        break;
      }
    case WL_CKPT_END:
      {
        dest->type = t;
        err_t_wrap (walf_read_ckpt_end (w, &checksum, dest, e), e);
        break;
      }
    default:
      {
        return error_causef (e, ERR_CORRUPT, "Invalid wal header type");
      }
    }

  walis_mark_end_log (&w->istream);

  i_log_trace ("PRead wal:\n");
  i_log_wal_rec_hdr_read (LOG_TRACE, dest);

  return SUCCESS;
}

err_t
walf_read (struct wal_rec_hdr_read *dest, lsn *rlsn, struct wal_file *w, error *e)
{
  DBG_ASSERT (wal_file, w);
  err_t_wrap (walf_lazy_istream_init (w, e), e);

  wlh t;
  u32 checksum = checksum_init ();

  // Do read or terminate early
  {
    walis_mark_start_log (&w->istream);
    bool iseof;
    err_t_wrap (walis_read_all (&w->istream, &iseof, rlsn, &checksum, &t, sizeof (t), e), e);
    if (iseof)
      {
        dest->type = WL_EOF;
        return SUCCESS;
      }
  }

  switch (t)
    {
    case WL_UPDATE:
      {
        dest->type = t;
        err_t_wrap (walf_read_update (w, &checksum, dest, e), e);
        break;
      }
    case WL_CLR:
      {
        dest->type = t;
        err_t_wrap (walf_read_clr (w, &checksum, dest, e), e);
        break;
      }
    case WL_BEGIN:
      {
        dest->type = t;
        err_t_wrap (walf_read_begin (w, &checksum, dest, e), e);
        break;
      }
    case WL_COMMIT:
      {
        dest->type = t;
        err_t_wrap (walf_read_commit (w, &checksum, dest, e), e);
        break;
      }
    case WL_END:
      {
        dest->type = t;
        err_t_wrap (walf_read_end (w, &checksum, dest, e), e);
        break;
      }
    case WL_CKPT_BEGIN:
      {
        dest->type = t;
        err_t_wrap (walf_read_ckpt_begin (w, &checksum, dest, e), e);
        break;
      }
    case WL_CKPT_END:
      {
        dest->type = t;
        err_t_wrap (walf_read_ckpt_end (w, &checksum, dest, e), e);
        break;
      }
    default:
      {
        err_t_wrap (error_causef (e, ERR_CORRUPT, "Invalid wal header type"), e);
        break;
      }
    }

  walis_mark_end_log (&w->istream);
  i_log_trace ("Read wal:\n");
  i_log_wal_rec_hdr_read (LOG_TRACE, dest);

  return SUCCESS;
}

err_t
walf_flush_to (struct wal_file *w, lsn l, error *e)
{
  DBG_ASSERT (wal_file, w);
  err_t_wrap (walf_lazy_ostream_init (w, e), e);
  return walos_flush_to (w->current_ostream, l, e);
}

#ifndef NTEST
err_t
walf_crash (struct wal_file *w, error *e)
{
  if (w->current_ostream)
    {
      err_t_wrap (walos_crash (w->current_ostream, e), e);
      w->current_ostream = NULL;
    }
  if (w->istream_open)
    {
      err_t_wrap (walis_crash (&w->istream, e), e);
      w->istream_open = false;
    }
  return SUCCESS;
}
#endif
