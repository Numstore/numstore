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
 *   Implements wal_rec_hdr.h. WAL record header operations including serialization and record type handling.
 */

#include "numstore/pager/dirty_page_table.h"
#include <numstore/pager/wal_rec_hdr.h>

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/intf/logging.h>
#include <numstore/pager/page.h>
#include <numstore/pager/txn_table.h>

const char *
wal_rec_hdr_type_tostr (enum wal_rec_hdr_type type)
{
  switch (type)
    {
    case WL_UPDATE:
      {
        return "WL_UPDATE";
      }
    case WL_CLR:
      {
        return "WL_CLR";
      }
    case WL_BEGIN:
      {
        return "WL_BEGIN";
      }
    case WL_COMMIT:
      {
        return "WL_COMMIT";
      }
    case WL_END:
      {
        return "WL_END";
      }
    case WL_CKPT_BEGIN:
      {
        return "WL_CKPT_BEGIN";
      }
    case WL_CKPT_END:
      {
        return "WL_CKPT_END";
      }
    case WL_EOF:
      {
        return "WL_EOF";
      }
    }

  UNREACHABLE ();
}

struct wal_rec_hdr_write
wrhw_from_wrhr (struct wal_rec_hdr_read *src)
{
  switch (src->type)
    {
    case WL_BEGIN:
      {
        return (struct wal_rec_hdr_write){
          .type = WL_BEGIN,
          .begin = src->begin,
        };
      }
    case WL_COMMIT:
      {
        return (struct wal_rec_hdr_write){
          .type = WL_COMMIT,
          .commit = src->commit,
        };
      }
    case WL_END:
      {
        return (struct wal_rec_hdr_write){
          .type = WL_END,
          .end = src->end,
        };
      }
    case WL_UPDATE:
      {
        return (struct wal_rec_hdr_write){
          .type = WL_UPDATE,
          .update = {
              .tid = src->update.tid,
              .prev = src->update.prev,
              .pg = src->update.pg,
              .redo = src->update.redo,
              .undo = src->update.undo,
          },
        };
      }
    case WL_CLR:
      {
        return (struct wal_rec_hdr_write){
          .type = WL_CLR,
          .clr = {
              .tid = src->clr.tid,
              .prev = src->clr.prev,
              .pg = src->clr.pg,
              .undo_next = src->clr.undo_next,
              .redo = src->clr.redo,
          },
        };
      }
    case WL_CKPT_BEGIN:
      {
        return (struct wal_rec_hdr_write){
          .type = WL_CKPT_BEGIN,
        };
      }
    case WL_CKPT_END:
      {
        return (struct wal_rec_hdr_write){
          .type = WL_CKPT_END,
          .ckpt_end = {
              .att = &src->ckpt_end.att,
              .dpt = &src->ckpt_end.dpt,
          },
        };
      }
    case WL_EOF:
      {
        UNREACHABLE ();
      }
    }
  UNREACHABLE ();
}

stxid
wrh_get_tid (struct wal_rec_hdr_read *h)
{
  switch (h->type)
    {
    case WL_BEGIN:
      {
        return h->begin.tid;
      }
    case WL_COMMIT:
      {
        return h->commit.tid;
      }
    case WL_END:
      {
        return h->end.tid;
      }
    case WL_UPDATE:
      {
        return h->update.tid;
      }
    case WL_CLR:
      {
        return h->clr.tid;
      }
    case WL_CKPT_BEGIN:
      {
        return -1;
      }
    case WL_CKPT_END:
      {
        return -1;
      }
    case WL_EOF:
      {
        UNREACHABLE ();
      }
    }
  UNREACHABLE ();
}

slsn
wrh_get_prev_lsn (struct wal_rec_hdr_read *h)
{
  switch (h->type)
    {
    case WL_BEGIN:
      {
        return 0;
      }
    case WL_COMMIT:
      {
        return h->commit.prev;
      }
    case WL_END:
      {
        return h->end.prev;
      }
    case WL_UPDATE:
      {
        return h->update.prev;
      }
    case WL_CLR:
      {
        return h->clr.prev;
      }
    case WL_CKPT_BEGIN:
      {
        return -1;
      }
    case WL_CKPT_END:
      {
        return -1;
      }
    case WL_EOF:
      {
        UNREACHABLE ();
      }
    }
  UNREACHABLE ();
}

#ifndef NTEST
bool
wal_rec_hdr_read_equal (struct wal_rec_hdr_read *left, struct wal_rec_hdr_read *right)
{
  bool match = true;

  match = match && left->type == right->type;

  i_log_debug (
      "Checking equality of wal header left: %s, right: %s\n",
      wal_rec_hdr_type_tostr (left->type),
      wal_rec_hdr_type_tostr (right->type));

  switch (left->type)
    {
    case WL_UPDATE:
      {
        match = match && left->update.tid == right->update.tid;
        match = match && left->update.prev == right->update.prev;
        match = match && left->update.pg == right->update.pg;
        match = match && i_memcmp (left->update.undo, right->update.undo, PAGE_SIZE) == 0;
        match = match && i_memcmp (left->update.redo, right->update.redo, PAGE_SIZE) == 0;
        break;
      }

    case WL_CLR:
      {
        match = match && left->clr.tid == right->clr.tid;
        match = match && left->clr.prev == right->clr.prev;
        match = match && left->clr.pg == right->clr.pg;
        match = match && left->clr.undo_next == right->clr.undo_next;
        match = match && i_memcmp (left->clr.redo, right->clr.redo, PAGE_SIZE) == 0;
        break;
      }

    case WL_BEGIN:
      {
        match = match && left->begin.tid == right->begin.tid;
        break;
      }

    case WL_END:
      {
        match = match && left->end.tid == right->end.tid;
        match = match && left->end.prev == right->end.prev;
        break;
      }

    case WL_COMMIT:
      {
        match = match && left->commit.tid == right->commit.tid;
        match = match && left->commit.prev == right->commit.prev;
        break;
      }

    case WL_CKPT_BEGIN:
      {
        break;
      }

    case WL_CKPT_END:
      {
        match = match && txnt_equal (&left->ckpt_end.att, &right->ckpt_end.att);
        match = match && dpgt_equal (&left->ckpt_end.dpt, &right->ckpt_end.dpt);
        break;
      }

    default:
      {
        UNREACHABLE ();
      }
    }

  return match;
}
#endif

static inline void
i_log_update (int log_level, const struct wal_rec_hdr_read *r)
{
  i_log (log_level, "------------------ %s:\n", wal_rec_hdr_type_tostr (r->type));
  i_printf (log_level, "TID: %ld\n", r->update.tid);
  i_printf (log_level, "PREV: %ld\n", r->update.prev);
  i_printf (log_level, "PG: %ld\n", r->update.pg);
  page temp;

  i_printf (log_level, "UNDO: ");
  for (int i = 0; i < 10; ++i)
    {
      i_printf (log_level, "%X ", r->update.undo[i]);
    }
  i_printf (log_level, "... ");
  for (u32 i = PAGE_SIZE - 10; i < PAGE_SIZE; ++i)
    {
      i_printf (log_level, "%X ", r->update.undo[i]);
    }
  i_printf (log_level, "\n");
  i_memcpy (temp.raw, r->update.undo, PAGE_SIZE);
  temp.pg = r->update.pg;
  i_log_page (log_level, &temp);

  i_printf (log_level, "REDO: ");
  for (u32 i = 0; i < 10; ++i)
    {
      i_printf (log_level, "%X ", r->update.redo[i]);
    }
  i_printf (log_level, "... ");
  for (u32 i = PAGE_SIZE - 10; i < PAGE_SIZE; ++i)
    {
      i_printf (log_level, "%X ", r->update.redo[i]);
    }
  i_printf (log_level, "\n");
  i_memcpy (temp.raw, r->update.redo, PAGE_SIZE);
  temp.pg = r->update.pg;
  i_log_page (log_level, &temp);
  i_log (log_level, "------------------------------------\n");
}

static inline void
i_log_clr (int log_level, const struct wal_rec_hdr_read *r)
{
  i_log (log_level, "------------------ %s:\n", wal_rec_hdr_type_tostr (r->type));
  i_printf (log_level, "TID: %ld\n", r->clr.tid);
  i_printf (log_level, "PREV: %ld\n", r->clr.prev);
  i_printf (log_level, "PG: %ld\n", r->clr.pg);
  i_printf (log_level, "UNDO_NEXT: %ld\n", r->clr.undo_next);
  i_printf (log_level, "REDO: ");
  for (u32 i = 0; i < 10; ++i)
    {
      i_printf (log_level, "%X ", r->clr.redo[i]);
    }
  i_printf (log_level, "... ");
  for (u32 i = PAGE_SIZE - 10; i < PAGE_SIZE; ++i)
    {
      i_printf (log_level, "%X ", r->clr.redo[i]);
    }
  i_printf (log_level, "\n");
  i_log (log_level, "------------------------------------\n");
}

static inline void
i_log_begin (int log_level, const struct wal_rec_hdr_read *r)
{
  i_log (log_level, "------------------ %s:\n", wal_rec_hdr_type_tostr (r->type));
  i_printf (log_level, "HEADER: %d\n", r->type);
  i_printf (log_level, "TID: %ld\n", r->begin.tid);
  i_log (log_level, "------------------------------------\n");
}

static inline void
i_log_commit (int log_level, const struct wal_rec_hdr_read *r)
{
  i_log (log_level, "------------------ %s:\n", wal_rec_hdr_type_tostr (r->type));
  i_printf (log_level, "HEADER: %d\n", r->type);
  i_printf (log_level, "TID: %ld\n", r->commit.tid);
  i_printf (log_level, "PREV: %ld\n", r->commit.prev);
  i_log (log_level, "------------------------------------\n");
}

static inline void
i_log_end (int log_level, const struct wal_rec_hdr_read *r)
{
  i_log (log_level, "------------------ %s:\n", wal_rec_hdr_type_tostr (r->type));
  i_printf (log_level, "HEADER: %d\n", r->type);
  i_printf (log_level, "TID: %ld\n", r->end.tid);
  i_printf (log_level, "PREV: %ld\n", r->end.prev);
  i_log (log_level, "------------------------------------\n");
}

static inline void
i_log_ckpt_begin (int log_level, const struct wal_rec_hdr_read *r)
{
  i_log (log_level, "------------------ %s:\n", wal_rec_hdr_type_tostr (r->type));
  i_printf (log_level, "HEADER: %d\n", r->type);
  i_log (log_level, "------------------------------------\n");
}

static inline void
i_log_ckpt_end (int log_level, struct wal_rec_hdr_read *r)
{
  i_log (log_level, "------------------ %s:\n", wal_rec_hdr_type_tostr (r->type));
  i_printf (log_level, "HEADER: %d\n", r->type);
  i_log_txnt (log_level, &r->ckpt_end.att);
  i_log_dpgt (log_level, &r->ckpt_end.dpt);
  i_log (log_level, "------------------------------------\n");
}

void
i_log_wal_rec_hdr_read (int log_level, struct wal_rec_hdr_read *r)
{
  switch (r->type)
    {
    case WL_UPDATE:
      {
        i_log_update (log_level, r);
        break;
      }

    case WL_CLR:
      {
        i_log_clr (log_level, r);
        break;
      }

    case WL_BEGIN:
      {
        i_log_begin (log_level, r);
        break;
      }

    case WL_COMMIT:
      {
        i_log_commit (log_level, r);
        break;
      }
    case WL_END:
      {
        i_log_end (log_level, r);
        break;
      }

    case WL_CKPT_BEGIN:
      {
        i_log_ckpt_begin (log_level, r);
        break;
      }

    case WL_CKPT_END:
      {
        i_log_ckpt_end (log_level, r);
        break;
      }
    case WL_EOF:
      {
        i_log (log_level, "WL_EOF\n");
        break;
      }
    }
}

void
i_print_wal_rec_hdr_read_light (int log_level, struct wal_rec_hdr_read *r, lsn l)
{
  switch (r->type)
    {
    case WL_UPDATE:
      {
        i_printf (log_level, "%15" PRlsn "      UPDATE     [ txid = %8" PRtxid ", pg   = %8" PRpgno "                      ] --> %" PRlsn "\n",
                  l, r->update.tid, r->update.pg, r->update.prev);
        break;
      }

    case WL_CLR:
      {
        i_printf (log_level, "%15" PRlsn "      CLR        [ txid = %8" PRtxid ", pg   = %8" PRpgno ", undoNext = %8" PRpgno " ] --> %" PRlsn "\n",
                  l, r->clr.tid, r->clr.pg, r->clr.undo_next, r->clr.prev);
        break;
      }

    case WL_BEGIN:
      {
        i_printf (log_level, "%15" PRlsn "      BEGIN      [ txid = %8" PRtxid "                                       ]\n", l, r->begin.tid);
        break;
      }

    case WL_COMMIT:
      {
        i_printf (log_level, "%15" PRlsn "      COMMIT     [ txid = %8" PRtxid "                                       ] --> %" PRlsn "\n",
                  l, r->commit.tid, r->commit.prev);
        break;
      }
    case WL_END:
      {
        i_printf (log_level, "%15" PRlsn "      END        [ txid = %8" PRtxid "                                       ] --> %" PRlsn "\n",
                  l, r->end.tid, r->end.prev);
        break;
      }

    case WL_CKPT_BEGIN:
      {
        i_printf (log_level, "%15" PRlsn "      CKPT_BEGIN\n", l);
        break;
      }

    case WL_CKPT_END:
      {
        i_printf (log_level, "%15" PRlsn "      CKPT_END   [ natt = %8d, ndpt = %8d                      ]\n",
                  l, txnt_get_size (&r->ckpt_end.att), dpgt_get_size (&r->ckpt_end.dpt));
        break;
      }
    case WL_EOF:
      {
        i_printf (log_level, "%15" PRlsn "      WL_EOF\n", l);
        break;
      }
    }
}

/////////////////////////////////////////////
/// DECODE
void
walf_decode_update (struct wal_rec_hdr_read *r, const u8 buf[WL_UPDATE_LEN])
{
  ASSERT (r->type == WL_UPDATE);

  u32 head = sizeof (wlh);

  // TID
  i_memcpy (&r->update.tid, buf + head, sizeof (r->update.tid));
  head += sizeof (r->update.tid);

  // PREV
  i_memcpy (&r->update.prev, buf + head, sizeof (r->update.prev));
  head += sizeof (r->update.prev);

  // PG
  i_memcpy (&r->update.pg, buf + head, sizeof (r->update.pg));
  head += sizeof (r->update.pg);

  // UNDO
  i_memcpy (r->update.undo, buf + head, PAGE_SIZE);
  head += PAGE_SIZE;

  // REDO
  i_memcpy (r->update.redo, buf + head, PAGE_SIZE);
}

void
walf_decode_clr (struct wal_rec_hdr_read *r, const u8 buf[WL_CLR_LEN])
{
  ASSERT (r->type == WL_CLR);

  u32 head = sizeof (wlh);

  // TID
  i_memcpy (&r->clr.tid, buf + head, sizeof (r->clr.tid));
  head += sizeof (r->clr.tid);

  // PREV
  i_memcpy (&r->clr.prev, buf + head, sizeof (r->clr.prev));
  head += sizeof (r->clr.prev);

  // PG
  i_memcpy (&r->clr.pg, buf + head, sizeof (r->clr.pg));
  head += sizeof (r->clr.pg);

  // UNDO_NEXT (must be != 0 for CLR)
  i_memcpy (&r->clr.undo_next, buf + head, sizeof (r->clr.undo_next));
  head += sizeof (r->clr.undo_next);

  // REDO only
  i_memcpy (r->clr.redo, buf + head, PAGE_SIZE);
}

void
walf_decode_begin (struct wal_rec_hdr_read *r, const u8 buf[WL_BEGIN_LEN])
{
  ASSERT (r->type == WL_BEGIN);

  u32 head = sizeof (wlh);

  // TID
  i_memcpy (&r->begin.tid, buf + head, sizeof (r->begin.tid));
}

void
walf_decode_commit (struct wal_rec_hdr_read *r, const u8 buf[WL_COMMIT_LEN])
{
  ASSERT (r->type == WL_COMMIT);

  u32 head = sizeof (wlh);
  txid tid;
  lsn prev;

  // TID
  i_memcpy (&tid, buf + head, sizeof (txid));
  head += sizeof (txid);

  // PREV
  i_memcpy (&prev, buf + head, sizeof (lsn));

  r->commit.tid = tid;
  r->commit.prev = prev;
}

void
walf_decode_end (struct wal_rec_hdr_read *r, const u8 buf[WL_END_LEN])
{
  ASSERT (r->type == WL_END);

  u32 head = sizeof (wlh);
  txid tid;
  lsn prev;

  // TID
  i_memcpy (&tid, buf + head, sizeof (txid));
  head += sizeof (txid);

  // PREV
  i_memcpy (&prev, buf + head, sizeof (lsn));

  r->end.tid = tid;
  r->end.prev = prev;
}

void
walf_decode_ckpt_begin (struct wal_rec_hdr_read *r, const u8 buf[WL_CKPT_BEGIN_LEN])
{
  ASSERT (r->type == WL_CKPT_BEGIN);
}

err_t
walf_decode_ckpt_end (struct wal_rec_hdr_read *r, const u8 *buf, error *e)
{
  ASSERT (r->type == WL_CKPT_END);
  ASSERT (buf);

  u32 head = sizeof (wlh);
  u32 attsize;
  u32 dptsize;

  // attsize
  i_memcpy (&attsize, buf + head, sizeof (attsize));
  head += sizeof (attsize);

  // dptsize
  i_memcpy (&dptsize, buf + head, sizeof (dptsize));
  head += sizeof (dptsize);

  // att
  if (attsize > 0)
    {
      r->ckpt_end.txn_bank = i_malloc (txnlen_from_serialized (attsize), sizeof *r->ckpt_end.txn_bank, e);
    }
  else
    {
      r->ckpt_end.txn_bank = NULL;
    }
  if (txnt_deserialize (&r->ckpt_end.att, buf + head, r->ckpt_end.txn_bank, attsize, e))
    {
      i_free (r->ckpt_end.txn_bank);
      return e->cause_code;
    }
  head += attsize;

  // dpt
  if (dpgt_deserialize (&r->ckpt_end.dpt, buf + head, dptsize, e))
    {
      txnt_close (&r->ckpt_end.att);
      i_free (r->ckpt_end.txn_bank);
      return e->cause_code;
    }
  head += attsize;

  return SUCCESS;
}
