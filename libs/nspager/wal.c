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
 *   Implements wal.h. Core write-ahead log implementation with log record creation and recovery logic.
 */

#include <wal.h>

#include <numstore/core/alloc.h>
#include <numstore/core/assert.h>
#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/random.h>
#include <numstore/intf/logging.h>
#include <numstore/pager/dirty_page_table.h>
#include <numstore/pager/txn_table.h>
#include <numstore/pager/wal_file.h>
#include <numstore/pager/wal_rec_hdr.h>
#include <numstore/test/testing.h>

#include <config.h>

DEFINE_DBG_ASSERT (
    struct wal, wal, w, {
      ASSERT (w);
    })

//////////////////////////////////////////////////////////////
//////// Lifecycle

err_t
wal_open (struct wal *dest, const char *fname, error *e)
{
  err_t_wrap (walf_open (&dest->wf, fname, e), e);
  latch_init (&dest->latch);
  return SUCCESS;
}

err_t
wal_reset (struct wal *dest, error *e)
{
  err_t_wrap (walf_reset (&dest->wf, e), e);
  return SUCCESS;
}

err_t
wal_close (struct wal *w, error *e)
{
  DBG_ASSERT (wal, w);

  return walf_close (&w->wf, e);
}

err_t
wal_write_mode (struct wal *w, error *e)
{
  return walf_write_mode (&w->wf, e);
}

//////////////////////////////////////////////////////////////
//////// Append Primitives

slsn
wal_append_begin_log (struct wal *w, txid tid, error *e)
{
  latch_lock (&w->latch);

  DBG_ASSERT (wal, w);

  w->whdr.type = WL_BEGIN;
  w->whdr.begin = (struct wal_begin){
    .tid = tid,
  };

  slsn result = walf_write (&w->wf, &w->whdr, e);

  latch_unlock (&w->latch);

  return result;
}

slsn
wal_append_commit_log (struct wal *w, txid tid, lsn prev, error *e)
{
  latch_lock (&w->latch);

  DBG_ASSERT (wal, w);

  w->whdr.type = WL_COMMIT;
  w->whdr.commit = (struct wal_commit){
    .prev = prev,
    .tid = tid,
  };

  slsn result = walf_write (&w->wf, &w->whdr, e);

  latch_unlock (&w->latch);

  return result;
}

slsn
wal_append_end_log (struct wal *w, txid tid, lsn prev, error *e)
{
  latch_lock (&w->latch);

  DBG_ASSERT (wal, w);

  w->whdr.type = WL_END;
  w->whdr.end = (struct wal_end){
    .prev = prev,
    .tid = tid,
  };

  slsn result = walf_write (&w->wf, &w->whdr, e);

  latch_unlock (&w->latch);

  return result;
}

slsn
wal_append_update_log (struct wal *w, struct wal_update_write update, error *e)
{
  latch_lock (&w->latch);

  DBG_ASSERT (wal, w);

  w->whdr.type = WL_UPDATE;
  w->whdr.update = update;

  slsn result = walf_write (&w->wf, &w->whdr, e);

  latch_unlock (&w->latch);

  return result;
}

slsn
wal_append_clr_log (struct wal *w, struct wal_clr_write clr, error *e)
{
  latch_lock (&w->latch);

  DBG_ASSERT (wal, w);

  w->whdr.type = WL_CLR;
  w->whdr.clr = clr;

  slsn result = walf_write (&w->wf, &w->whdr, e);

  latch_unlock (&w->latch);

  return result;
}

slsn
wal_append_ckpt_begin (struct wal *w, error *e)
{
  latch_lock (&w->latch);

  DBG_ASSERT (wal, w);

  w->whdr.type = WL_CKPT_BEGIN;

  slsn result = walf_write (&w->wf, &w->whdr, e);

  latch_unlock (&w->latch);

  return result;
}

slsn
wal_append_ckpt_end (struct wal *w, struct txn_table *att, struct dpg_table *dpt, error *e)
{
  latch_lock (&w->latch);

  DBG_ASSERT (wal, w);

  w->whdr.type = WL_CKPT_END;

  w->whdr.ckpt_end = (struct wal_ckpt_end_write){
    .att = att,
    .dpt = dpt,
  };

  slsn result = walf_write (&w->wf, &w->whdr, e);

  latch_unlock (&w->latch);

  return result;
}

slsn
wal_append_log (struct wal *ww, struct wal_rec_hdr_write *whdr, error *e)
{
  switch (whdr->type)
    {
    case WL_BEGIN:
      {
        return wal_append_begin_log (ww, whdr->begin.tid, e);
      }
    case WL_COMMIT:
      {
        return wal_append_commit_log (ww, whdr->commit.tid, whdr->commit.prev, e);
      }
    case WL_END:
      {
        return wal_append_end_log (ww, whdr->end.tid, whdr->end.prev, e);
      }
    case WL_UPDATE:
      {
        return wal_append_update_log (ww, whdr->update, e);
      }
    case WL_CLR:
      {
        return wal_append_clr_log (ww, whdr->clr, e);
      }
    case WL_CKPT_BEGIN:
      {
        return wal_append_ckpt_begin (ww, e);
      }
    case WL_CKPT_END:
      {
        return wal_append_ckpt_end (ww, whdr->ckpt_end.att, whdr->ckpt_end.dpt, e);
      }
    case WL_EOF:
      {
        UNREACHABLE ();
      }
    }

  UNREACHABLE ();
}

//////////////////////////////////////////////////////////////
//////// Flush

err_t
wal_flush_to (struct wal *w, lsn l, error *e)
{
  DBG_ASSERT (wal, w);
  return walf_flush_to (&w->wf, l, e);
}

err_t
wal_flush_all (struct wal *w, error *e)
{
  DBG_ASSERT (wal, w);
  return walf_flush_all (&w->wf, e);
}

//////////////////////////////////////////////////////////////
//////// Read Primitive

struct wal_rec_hdr_read *
wal_read_next (struct wal *w, lsn *rlsn, error *e)
{
  latch_lock (&w->latch);

  DBG_ASSERT (wal, w);

  if (walf_read (&w->rhdr, rlsn, &w->wf, e) == SUCCESS)
    {
      latch_unlock (&w->latch);
      return &w->rhdr;
    }

  latch_unlock (&w->latch);
  return NULL;
}

struct wal_rec_hdr_read *
wal_read_entry (struct wal *w, lsn id, error *e)
{
  latch_lock (&w->latch);

  DBG_ASSERT (wal, w);

  if (walf_pread (&w->rhdr, &w->wf, id, e) == SUCCESS)
    {
      latch_unlock (&w->latch);
      return &w->rhdr;
    }

  latch_unlock (&w->latch);
  return NULL;
}

//////////////////////////////////////////////////////////////
//////// TESTS

#ifndef NTEST
TEST_disabled (TT_UNIT, wal)
{
  error e = error_create ();
  struct wal ww;
  struct txn_table att1;
  struct txn_table att2;
  struct dpg_table dpt1;
  struct dpg_table dpt2;

  test_err_t_wrap (i_remove_quiet ("test.wal", &e), &e);
  test_err_t_wrap (wal_open (&ww, "test.wal", &e), &e);

  decl_rand_buffer (undo1, u8, PAGE_SIZE);
  decl_rand_buffer (redo1, u8, PAGE_SIZE);

  decl_rand_buffer (redo2, u8, PAGE_SIZE);

  decl_rand_buffer (undo3, u8, PAGE_SIZE);
  decl_rand_buffer (redo3, u8, PAGE_SIZE);

  struct wal_rec_hdr_read in[] = {
    {
        .type = WL_BEGIN,
        .begin = {
            .tid = 1,
        },
    },
    {
        .type = WL_COMMIT,
        .commit = {
            .tid = 3,
            .prev = 20,
        },
    },
    {
        .type = WL_END,
        .end = {
            .tid = 4,
            .prev = 30,
        },
    },

    {
        .type = WL_UPDATE,
        .update = {
            .tid = 5,
            .pg = 111,
            .prev = 40,
        },
    },
    {
        .type = WL_CLR,
        .clr = {
            .tid = 6,
            .prev = 50,
            .pg = 222,
            .undo_next = 42,
        },
    },
    {
        .type = WL_CKPT_BEGIN,
    },
    {
        .type = WL_CKPT_END,
    },
    {
        .type = WL_CKPT_END,
    }
  };

  i_memcpy (in[3].update.undo, undo1, PAGE_SIZE);
  i_memcpy (in[3].update.redo, redo1, PAGE_SIZE);

  i_memcpy (in[4].update.redo, redo2, PAGE_SIZE);

  // Allocate and store in variables for proper cleanup
  test_err_t_wrap (txnt_open (&att1, &e), &e);
  test_err_t_wrap (dpgt_open (&dpt1, &e), &e);

  test_err_t_wrap (txnt_open (&att2, &e), &e);
  test_err_t_wrap (dpgt_open (&dpt2, &e), &e);

  in[6].ckpt_end.att = att1;
  in[6].ckpt_end.dpt = dpt1;
  in[7].ckpt_end.att = att2;
  in[7].ckpt_end.dpt = dpt2;

  // Add random data to the tables
  struct alloc a;
  a.type = AT_CHNK_ALLOC;
  chunk_alloc_create_default (&a._calloc);
  test_err_t_wrap (txnt_determ_populate (&in[6].ckpt_end.att, &a, &e), &e);
  test_err_t_wrap (dpgt_rand_populate (&in[6].ckpt_end.dpt, &e), &e);
  test_err_t_wrap (txnt_rand_populate (&in[7].ckpt_end.att, &a, &e), &e);
  test_err_t_wrap (dpgt_rand_populate (&in[7].ckpt_end.dpt, &e), &e);

  struct wal_rec_hdr_read in2[] = {
    {
        .type = WL_BEGIN,
        .begin = {
            .tid = 2,
        },
    },

    // tid, prev, pg, undo, redo
    {
        .type = WL_UPDATE,
        .update = {
            .tid = 6,
            .pg = 112,
            .prev = 41,
        },
    },
  };

  i_memcpy (in2[1].update.undo, undo3, PAGE_SIZE);
  i_memcpy (in2[1].update.redo, redo3, PAGE_SIZE);

  // WRITE x1
  {
    // Write them out
    slsn l = 0;

    for (u32 i = 0; i < arrlen (in); i++)
      {
        struct wal_rec_hdr_write out = wrhw_from_wrhr (&in[i]);
        l = wal_append_log (&ww, &out, &e);
        test_err_t_wrap (l, &e);
      }
  }

  // READ x1
  {
    // Read them from the start
    for (u32 i = 0; i < arrlen (in); i++)
      {
        lsn read_lsn;
        struct wal_rec_hdr_read *next = wal_read_next (&ww, &read_lsn, &e);
        test_fail_if_null (next);
        test_assert (wal_rec_hdr_read_equal (next, &in[i]));

        // Free deserialized CKPT_END records
        if (next->type == WL_CKPT_END)
          {
            txnt_close (&next->ckpt_end.att);
            i_free (next->ckpt_end.txn_bank);
            dpgt_close (&next->ckpt_end.dpt);
          }
      }
  }

  // WRITE x2
  {
    test_err_t_wrap (wal_write_mode (&ww, &e), &e);

    slsn l = 0;

    // Second batch write more
    for (u32 i = 0; i < arrlen (in2); i++)
      {
        struct wal_rec_hdr_write out = wrhw_from_wrhr (&in[i]);
        l = wal_append_log (&ww, &out, &e);
        test_err_t_wrap (l, &e);
      }
  }

  // READ x2
  {
    // Read first half
    for (u32 i = 0; i < arrlen (in); i++)
      {
        lsn read_lsn;
        struct wal_rec_hdr_read *next = wal_read_next (&ww, &read_lsn, &e);
        test_fail_if_null (next);
        test_assert (wal_rec_hdr_read_equal (next, &in[i]));

        // Free deserialized CKPT_END records
        if (next->type == WL_CKPT_END)
          {
            txnt_close (&next->ckpt_end.att);
            dpgt_close (&next->ckpt_end.dpt);
          }
      }

    // Read second half
    for (u32 i = 0; i < arrlen (in2); i++)
      {
        lsn read_lsn;
        struct wal_rec_hdr_read *next = wal_read_next (&ww, &read_lsn, &e);
        test_fail_if_null (next);
        test_assert (wal_rec_hdr_read_equal (next, &in2[i]));

        if (next->type == WL_CKPT_END)
          {
            txnt_close (&next->ckpt_end.att);
            dpgt_close (&next->ckpt_end.dpt);
          }
      }
  }

  txnt_close (&att1);
  dpgt_close (&dpt1);
  txnt_close (&att2);
  dpgt_close (&dpt2);

  test_err_t_wrap (wal_close (&ww, &e), &e);
}
#endif

#ifndef NTEST
err_t
wal_crash (struct wal *w, error *e)
{
  DBG_ASSERT (wal, w);

  return walf_crash (&w->wf, e);
}
#endif
