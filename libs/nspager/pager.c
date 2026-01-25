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
 *   Implements pager.h. Main pager implementation with transaction support, page cache, and WAL integration.
 */

#include <file_pager.h>

#include <numstore/core/adptv_hash_table.h>
#include <numstore/core/assert.h>
#include <numstore/core/dbl_buffer.h>
#include <numstore/core/error.h>
#include <numstore/core/latch.h>
#include <numstore/core/max_capture.h>
#include <numstore/core/random.h>
#include <numstore/core/string.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/intf/types.h>
#include <numstore/pager.h>
#include <numstore/pager/data_list.h>
#include <numstore/pager/dirty_page_table.h>
#include <numstore/pager/lock_table.h>
#include <numstore/pager/lt_lock.h>
#include <numstore/pager/page.h>
#include <numstore/pager/page_h.h>
#include <numstore/pager/root_node.h>
#include <numstore/pager/tombstone.h>
#include <numstore/pager/txn.h>
#include <numstore/pager/txn_table.h>
#include <numstore/pager/wal_file.h>
#include <numstore/pager/wal_stream.h>
#include <numstore/test/page_fixture.h>
#include <numstore/test/testing.h>

#include <_pager.h>
#include <aries.h>
#include <config.h>
#include <wal.h>

/**
 * Flush this page frame to non volatile storage
 */
static inline err_t
pgr_flush (struct pager *p, struct page_frame *mp, error *e)
{
  DBG_ASSERT (pager, p);
  ASSERT (pf_check (mp, PW_PRESENT));
  ASSERTF (!pf_check (mp, PW_X),
           "Trying to flush a page: %" PRpgno " currently being modified in X mode",
           mp->page.pg);

  // Only need to write it out if it's dirty
  if (pf_check (mp, PW_DIRTY))
    {
      i_log_trace ("Flushing page: %" PRpgno " from the pager\n", mp->page.pg);
      i_log_trace ("Page: %" PRpgno " is dirty, flushing to wal\n", mp->page.pg);

      /**
       * WAL Invariant: Flush to wal before flushing to disk
       *
       * When we're in analysis phase we're reading and there's
       * no point in writing pages during evict because we SHOULDN't
       * be creating new data.
       */
      if (!p->restarting)
        {
          lsn plsn = page_get_page_lsn (&mp->page);
          err_t_wrap (wal_flush_to (&p->ww, plsn, e), e);
        }

      i_log_trace ("Page: %" PRpgno " flushed to wal, writing to file now\n", mp->page.pg);
      err_t_wrap (fpgr_write (&p->fp, mp->page.raw, mp->page.pg, e), e);

      pf_clr (mp, PW_DIRTY);

      err_t_wrap (dpgt_remove_expect (&p->dpt, mp->page.pg, e), e);
    }

  return SUCCESS;
}

static inline err_t
pgr_evict (struct pager *p, struct page_frame *mp, error *e)
{
  DBG_ASSERT (pager, p);
  ASSERT (pf_check (mp, PW_PRESENT));
  ASSERT (!pf_check (mp, PW_X));
  ASSERT (mp->pin == 0);

  err_t_wrap (pgr_flush (p, mp, e), e);

  ht_delete_expect_idx (&p->pgno_to_value, NULL, mp->page.pg);
  mp->flags = 0;
  pf_clr (mp, PW_PRESENT);

  return SUCCESS;
}

static inline err_t
pgr_evict_all (struct pager *pg, error *e)
{
  // Should be called when there are no PW_X in the db
  DBG_ASSERT (pager, pg);

  for (u32 i = 0; i < MEMORY_PAGE_LEN; ++i)
    {
      struct page_frame *mp = &pg->pages[pg->clock];

      if (pf_check (mp, PW_PRESENT))
        {
          pgr_evict (pg, mp, e);
        }

      pg->clock = (pg->clock + 1) % MEMORY_PAGE_LEN;
    }

  return e->cause_code;
}

static inline err_t
pgr_flush_all (struct pager *pg, error *e)
{
  DBG_ASSERT (pager, pg);

  for (u32 i = 0; i < MEMORY_PAGE_LEN; ++i)
    {
      struct page_frame *mp = &pg->pages[pg->clock];

      if (pf_check (mp, PW_PRESENT) && !pf_check (mp, PW_X))
        {
          pgr_flush (pg, mp, e);
        }

      pg->clock = (pg->clock + 1) % MEMORY_PAGE_LEN;
    }

  return e->cause_code;
}

static inline err_t
pgr_reserve_at_clock_thread_unsafe (struct pager *p, error *e)
{
  DBG_ASSERT (pager, p);

  i_log_trace ("Pager reserving a spot in buffer pool\n");

  // 2 times so that we might clear an access bit
  struct page_frame *mp;
  for (u32 i = 0; i < 2 * MEMORY_PAGE_LEN; ++i)
    {
      i_log_trace ("Reserve: checking page: %u\n", p->clock);

      mp = &p->pages[p->clock];

      // Found an empty spot
      if (!pf_check (mp, PW_PRESENT))
        {
          i_log_trace ("Page: %u is not present, using this spot\n", p->clock);
          goto found_spot;
        }

      // Pinned, skip it
      if (mp->pin > 0)
        {
          i_log_trace ("Page: %u is pinned with pin: %u, using this spot\n", p->clock, mp->pin);
          p->clock = (p->clock + 1) % MEMORY_PAGE_LEN;
          continue;
        }

      // Access bit is on - set off and continue
      if (pf_check (mp, PW_ACCESS))
        {
          i_log_trace ("Page: %u has access bit: 1, clearing then skipping\n", p->clock);
          pf_clr (mp, PW_ACCESS);
          p->clock = (p->clock + 1) % MEMORY_PAGE_LEN;
          continue;
        }

      // EVICT
      i_log_trace ("Page: %u is present but doesn't have access bit, evicting\n", p->clock);
      err_t_wrap (pgr_evict (p, mp, e), e);
      goto found_spot;
    }

  return error_causef (e, ERR_PAGER_FULL, "Memory buffer pool is full");

found_spot:
  ASSERT (!pf_check (&p->pages[p->clock], PW_PRESENT));
  return SUCCESS;
}

static err_t
pgr_new_extend (page_h *dest, struct pager *p, struct txn *tx, error *e)
{
  DBG_ASSERT (pager, p);
  DBG_ASSERT (page_h, dest);
  ASSERT (dest->mode == PHM_NONE);

  i_log_trace ("Creating new page by extending file\n");

  pgno pg;
  struct page_frame *pgr = NULL, *pgw = NULL;
  u32 pgrloc;

  err_t ret = SUCCESS;

  // Reserve the read page spot
  latch_lock (&p->l);
  {
    ret = pgr_reserve_at_clock_thread_unsafe (p, e);
    if (ret)
      {
        latch_unlock (&p->l);
        goto theend;
      }
    pgrloc = p->clock;

    pgr = &p->pages[pgrloc];
    pgr->pin = 1;
    pgr->flags = 0;
    pgr->wsibling = -1;
    pf_set (pgr, PW_ACCESS);
    pf_set (pgr, PW_PRESENT);
  }
  latch_unlock (&p->l);

  page_init_empty (&pgr->page, PG_TOMBSTONE);
  tmbst_set_next (&pgr->page, fpgr_get_npages (&p->fp) + 1);

  i_printf_trace ("Read buffer pool location: %d\n", pgrloc);

  // Reserve the write page spot
  latch_lock (&p->l);
  {
    ret = pgr_reserve_at_clock_thread_unsafe (p, e);
    if (ret)
      {
        pgr->pin = 0;
        pgr->flags = 0;
        latch_unlock (&p->l);
        goto theend;
      }

    pgr->wsibling = p->clock;

    pgw = &p->pages[p->clock];
    pgw->pin = 1;
    pgw->flags = 0;
    pgw->wsibling = -1;
    pf_set (pgw, PW_PRESENT);
    pf_set (pgw, PW_X);
  }
  latch_unlock (&p->l);

  i_printf_trace ("Write buffer pool location: %d\n", pgr->wsibling);

  /**
   * Now we can actually extend the file
   * TODO - You don't need to do this and we
   * could reduce system calls by lazily extending the
   * file and truncating it once on large pages
   */
  ret = fpgr_new (&p->fp, &pg, e);
  if (ret)
    {
      latch_lock (&p->l);
      pgr->pin = 0;
      pgr->flags = 0;
      pgw->pin = 0;
      pgw->flags = 0;
      latch_unlock (&p->l);
      goto theend;
    }

  i_printf_trace ("New page number: %" PRpgno "\n", pg);
  i_memcpy (pgw->page.raw, pgr->page.raw, PAGE_SIZE);
  pgr->page.pg = pg;
  pgw->page.pg = pg;

  // Mark page as dirty (will be added to DPT in pgr_save with proper LSN)
  pf_set (pgr, PW_DIRTY);

  // Insert page into the hash table
  hdata_idx hd = (hdata_idx){ .key = pg, .value = pgrloc };
  ht_insert_expect_idx (&p->pgno_to_value, hd);

  // Initialize page_h
  dest->pgr = pgr;
  dest->pgw = pgw;
  dest->mode = PHM_X;
  dest->tx = tx;

theend:
  i_log_trace ("Done trying to extend new page. Exit code: %d\n", ret);
  return ret;
}

///////////////////////////////////////////////////////////
////// LIFECYCLE

static struct pager *
pgr_open_new (const char *fname, const char *walname, struct pager *p, error *e)
{
  i_log_info ("Creating new database: %s\n", fname);
  if (fpgr_reset (&p->fp, e))
    {
      return NULL;
    }
  if (wal_reset (&p->ww, e))
    {
      return NULL;
    }

  if (txnt_open (&p->tnxt, e))
    {
      return NULL;
    }

  p->next_tid = 1;

  // The first transaction - create the root page
  {
    struct txn tx;

    if (pgr_begin_txn (&tx, p, e))
      {
        return NULL;
      }

    page_h root = page_h_create ();
    pgr_new_extend (&root, p, &tx, e);
    page_init_empty (page_h_w (&root), PG_ROOT_NODE);

    // Save the root page to WAL before releasing
    err_t_wrap_null_goto (pgr_save (p, &root, PG_ROOT_NODE, e) == SUCCESS ? p : NULL, failed, e);
    pgr_release (p, &root, PG_ROOT_NODE, e);

    err_t ret = pgr_commit (p, &tx, e);
    if (ret)
      {
        goto failed;
      }
  }

  return p;

failed:
  txnt_close (&p->tnxt);
  return NULL;
}

static struct pager *
pgr_open_existing (const char *fname, const char *walname, struct pager *ret, error *e)
{
  i_log_info ("Opening existing database: %s\n", fname);

  // Open the transaction table
  if (txnt_open (&ret->tnxt, e))
    {
      return NULL;
    }

  // Run ARIES recovery if WAL is enabled
  i_log_info ("Running ARIES recovery (analysis/redo/undo)...\n");

  i_log_info ("Read master LSN: %" PRlsn "\n", ret->master_lsn);

  if (ret->master_lsn == 0)
    {
      i_log_info ("No checkpoint found, starting recovery from beginning\n");
    }
  else
    {
      i_log_info ("Starting recovery from checkpoint at LSN %" PRlsn "\n", ret->master_lsn);
    }

  struct aries_ctx ctx;
  if (aries_ctx_create (&ctx, ret->master_lsn, e))
    {
      goto failed;
    }
  err_t result = pgr_restart (ret, &ctx, e);
  // Go back to write mode
  if (result != SUCCESS)
    {
      i_log_error ("ARIES recovery failed with error code %d: %s\n", e->cause_code, e->cause_msg);
      goto failed;
    }

  i_log_info ("ARIES recovery completed successfully\n");

  // Enter optimized write mode
  err_t_wrap_goto (wal_write_mode (&ret->ww, e), failed, e);
  ret->next_tid = ctx.max_tid + 1;

  i_log_info ("Opened existing database, starting with next_tid: %" PRtxid "\n", ret->next_tid);

  return ret;

failed:
  txnt_close (&ret->tnxt);
  return NULL;
}

static inline err_t
pgr_is_new_guard (struct pager *p, bool *isnew, error *e)
{
  *isnew = false;

  if (fpgr_get_npages (&p->fp) == 0)
    {
      *isnew = true;
      return SUCCESS;
    }

  page root;

  err_t_wrap (fpgr_read (&p->fp, root.raw, ROOT_PGNO, e), e);
  root.pg = ROOT_PGNO;

  p->master_lsn = rn_get_master_lsn (&root);
  p->first_tombstone = rn_get_first_tmbst (&root);

  return SUCCESS;
}

struct pager *
pgr_open (const char *fname, const char *walname, struct lockt *lt, struct thread_pool *tp, error *e)
{
  struct pager *ret = NULL;
  bool is_new = !i_exists_rw (fname);
  bool fpgr_opened = false;
  bool dpt_opened = false;
  bool wal_opened = false;

  // Allocate the pager
  if ((ret = i_calloc (1, sizeof *ret, e)) == NULL)
    {
      return NULL;
    }

  // Initialize the file pager
  err_t_wrap_goto (fpgr_open (&ret->fp, fname, e), failed, e);
  fpgr_opened = true;

  // Pull in the root node data values
  err_t_wrap_goto (pgr_is_new_guard (ret, &is_new, e), failed, e);

  // Open the dirty page table
  err_t_wrap_goto (dpgt_open (&ret->dpt, e), failed, e);
  dpt_opened = true;

  // Initialize the WAL
  if (walname)
    {
      err_t_wrap_goto (wal_open (&ret->ww, walname, e), failed, e);
      wal_opened = true;
    }
  else
    {
      panic ("TODO - optional WAL");
    }

  // Initialize the hash table from pgno -> table index
  ht_init_idx (&ret->pgno_to_value, ret->_hdata, MEMORY_PAGE_LEN);

  // Initialize internal latch
  latch_init (&ret->l);

  // Initialize page frame latches
  for (u32 i = 0; i < MEMORY_PAGE_LEN; ++i)
    {
      latch_init (&ret->pages[i].latch);
    }

  // Simple variables
  ret->clock = 0;
  ret->next_tid = 1;
  ret->lt = lt;
  ret->tp = tp;

  // More work
  if (is_new)
    {
      err_t_wrap_null_goto (pgr_open_new (fname, walname, ret, e), failed, e);
    }
  else
    {
      err_t_wrap_null_goto (pgr_open_existing (fname, walname, ret, e), failed, e);
    }

  DBG_ASSERT (pager, ret);
  return ret;

failed:
  ASSERT (e->cause_code);
  // latch doesn't need cleanup
  if (ret && dpt_opened)
    {
      dpgt_close (&ret->dpt);
    }
  if (ret && wal_opened)
    {
      wal_close (&ret->ww, e);
    }
  if (ret && fpgr_opened)
    {
      fpgr_close (&ret->fp, e);
    }
  if (ret)
    {
      i_free (ret);
    }
  if (is_new)
    {
      i_remove_quiet (fname, e);
      i_remove_quiet (walname, e);
    }
  return NULL;
}

#ifndef NTEST
TEST (TT_UNIT, pager_open)
{
  error e = error_create ();

  /* Green path */
  {
    test_fail_if (i_remove_quiet ("test.db", &e));
    test_fail_if (i_remove_quiet ("test.wal", &e));

    struct lockt lt;
    test_err_t_wrap (lockt_init (&lt, &e), &e);

    struct thread_pool *tp = tp_open (&e);
    test_fail_if_null (tp);

    struct pager *p = pgr_open ("test.db", "test.wal", &lt, tp, &e);

    test_fail_if_null (p);
    test_err_t_wrap (pgr_close (p, &e), &e);

    test_err_t_wrap (tp_free (tp, &e), &e);
    lockt_destroy (&lt);
  }
}
#endif

#ifndef NTEST
TEST (TT_UNIT, pgr_open_basic)
{
  error e = error_create ();
  test_fail_if (i_remove_quiet ("test.db", &e));
  test_fail_if (i_remove_quiet ("test.wal", &e));

  struct lockt lt;
  test_err_t_wrap (lockt_init (&lt, &e), &e);

  struct thread_pool *tp = tp_open (&e);
  test_fail_if_null (tp);

  i_file fp;
  test_err_t_wrap (i_open_rw (&fp, "test.db", &e), &e);

  struct pager *p;

  /* File is shorter than page size */
  test_fail_if (i_truncate (&fp, PAGE_SIZE - 1, &e));
  p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_assert_int_equal (e.cause_code, ERR_CORRUPT);
  test_assert_equal (p, NULL);
  e.cause_code = SUCCESS;

  /* Half a page */
  test_fail_if (i_truncate (&fp, PAGE_SIZE / 2, &e));
  p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_assert_int_equal (e.cause_code, ERR_CORRUPT);
  test_assert_equal (p, NULL);
  e.cause_code = SUCCESS;

  /* 0 pages */
  test_fail_if (i_truncate (&fp, 0, &e));
  p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_assert_int_equal (e.cause_code, SUCCESS);
  test_fail_if_null (p);
  test_assert_int_equal ((int)pgr_get_npages (p), 1);
  test_fail_if (pgr_close (p, &e));

  /* Tear down */
  test_fail_if (i_close (&fp, &e));
  test_fail_if (i_unlink ("test.db", &e));

  test_err_t_wrap (tp_free (tp, &e), &e);
  lockt_destroy (&lt);
}
#endif

err_t
pgr_close (struct pager *p, error *e)
{
  DBG_ASSERT (pager, p);

  // Save all in memory pages
  pgr_evict_all (p, e);

  wal_close (&p->ww, e);
  fpgr_close (&p->fp, e);

  txnt_close (&p->tnxt);
  dpgt_close (&p->dpt);

  i_free (p);

  return e->cause_code;
}

#ifndef NTEST
TEST (TT_UNIT, pgr_close_success)
{
  error e = error_create ();
  test_fail_if (i_remove_quiet ("test.db", &e));
  test_fail_if (i_remove_quiet ("test.wal", &e));

  struct lockt lt;
  test_err_t_wrap (lockt_init (&lt, &e), &e);

  struct thread_pool *tp = tp_open (&e);
  test_fail_if_null (tp);

  struct pager *p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  test_fail_if_null (p);

  /* Delete file i_close should fail */
  test_assert_equal (pgr_close (p, &e), SUCCESS);
  test_fail_if (i_remove_quiet ("foo.db", &e));

  test_err_t_wrap (tp_free (tp, &e), &e);
  lockt_destroy (&lt);
}
#endif

///////////////////////////////////////////////////////////
////// COMMON

p_size
pgr_get_npages (const struct pager *p)
{
  DBG_ASSERT (pager, p);
  return fpgr_get_npages (&p->fp);
}

///////////////////////////////////////////////////////////
////// TRANSACTION CONTROL

err_t
pgr_begin_txn (struct txn *tx, struct pager *p, error *e)
{
  DBG_ASSERT (pager, p);

  // Generate a new transaction ID
  latch_lock (&p->l);
  txid tid = p->next_tid++;
  latch_unlock (&p->l);

  // Append begin record
  slsn l = wal_append_begin_log (&p->ww, tid, e);
  if (l < 0)
    {
      return e->cause_code;
    }

  txn_init (tx, tid, (struct txn_data){
                         .last_lsn = l,
                         .undo_next_lsn = 0,
                         .state = TX_RUNNING,
                     });

  // Create a new transaction entry
  err_t_wrap (txnt_insert_txn (&p->tnxt, tx, e), e);

  return SUCCESS;
}

err_t
pgr_commit (struct pager *p, struct txn *tx, error *e)
{
  DBG_ASSERT (pager, p);

  latch_lock (&tx->l);

  if (tx->data.state != TX_RUNNING)
    {
      latch_unlock (&tx->l);
      return error_causef (e, ERR_DUPLICATE_COMMIT, "Committing a transaction that is already committed\n");
    }

  // Append a commit log for this transaction
  slsn l = wal_append_commit_log (&p->ww, tx->tid, tx->data.last_lsn, e);
  if (l < 0)
    {
      latch_unlock (&tx->l);
      return e->cause_code;
    }

  // Flush the wal to the expected lsn
  err_t_wrap (wal_flush_to (&p->ww, l, e), e);

  // Append an end log to the wal
  l = wal_append_end_log (&p->ww, tx->tid, l, e);
  if (l < 0)
    {
      latch_unlock (&tx->l);
      return e->cause_code;
    }

  err_t_wrap (txnt_remove_txn_expect (&p->tnxt, tx, e), e);

  tx->data.state = TX_DONE;

  err_t_wrap (lockt_unlock (p->lt, tx, e), e);

  latch_unlock (&tx->l);

  return SUCCESS;
}

static err_t
pgr_update_master_lsn (struct pager *p, lsn mlsn, error *e)
{
  // BEGIN TXN
  struct txn tx;
  err_t_wrap (pgr_begin_txn (&tx, p, e), e);

  // X(root)
  if (lockt_lock (p->lt, (struct lt_lock){ .type = LOCK_ROOT, .data = { 0 } }, LM_X, &tx, e))
    {
      goto theend;
    }

  // GET ROOT
  page_h root = page_h_create ();
  if (pgr_get_writable (&root, &tx, PG_ROOT_NODE, 0, p, e))
    {
      return e->cause_code;
    }

  // UPDATE MLSN
  rn_set_master_lsn (page_h_w (&root), mlsn);

  // SAVE (don't release yet, we'll evict it)
  if (pgr_save (p, &root, PG_ROOT_NODE, e))
    {
      goto theend;
    }

  // COMMIT
  if (pgr_commit (p, &tx, e))
    {
      goto theend;
    }

  /**
   * Flush root node to disk. It doesn't hurt. Technically we don't need to
   * but if we want checkpoint to be "done" after this call, root page should be persisted (forced)
   * to disk
   */
  if (pgr_flush (p, root.pgr, e))
    {
      goto theend;
    }

theend:
  pgr_release (p, &root, PG_ROOT_NODE, e);
  return e->cause_code;
}

err_t
pgr_checkpoint (struct pager *p, error *e)
{
  // BEGIN CHECKPOINT
  slsn mlsn = wal_append_ckpt_begin (&p->ww, e);
  err_t_wrap (mlsn, e);

  // FLUSH PAGES
  err_t_wrap (pgr_evict_all (p, e), e);

  // END CHECKPOINT
  slsn end_lsn = wal_append_ckpt_end (&p->ww, &p->tnxt, &p->dpt, e);
  if (end_lsn < 0)
    {
      return e->cause_code;
    }

  // Flush the wal so that master lsn is accurate
  err_t_wrap (wal_flush_to (&p->ww, end_lsn, e), e);

  // Update master lsn
  err_t_wrap (pgr_update_master_lsn (p, mlsn, e), e);

  i_log_info ("Checkpoint written at LSN %" PRlsn "\n", mlsn);

  return SUCCESS;
}

/////////////////////////////////////////
//// READ / WRITE PAGES

err_t
pgr_get (page_h *dest, int flags, pgno pg, struct pager *p, error *e)
{
  DBG_ASSERT (page_h, dest);
  ASSERT (dest->mode == PHM_NONE);

  struct page_frame *pgr = NULL;

  err_t ret = SUCCESS;

  // Try to fetch from memory first
  hdata_idx data;
  switch (ht_get_idx (&p->pgno_to_value, &data, pg))
    {
    case HTAR_SUCCESS:
      {
        pgr = &p->pages[data.value];

        // TODO BLOCK ON X
        if (pgr->wsibling >= 0)
          {
            ASSERT (0 && "WOULD BLOCK ON X!");
          }

        // No operation would have let a pgr into an invalid state
        ASSERT (page_validate_for_db (&pgr->page, flags, NULL) == SUCCESS);
        pgr->pin++;
        break;
      }
    case HTAR_DOESNT_EXIST:
      {
        // Then load into a new spot
        ret = pgr_reserve_at_clock_thread_unsafe (p, e);
        if (ret)
          {
            return ret;
          }

        // Read in data to the current page
        pgr = &p->pages[p->clock];

        ret = fpgr_read (&p->fp, pgr->page.raw, pg, e);
        if (ret)
          {
            return ret;
          }

        ret = page_validate_for_db (&pgr->page, flags, e);
        if (ret)
          {
            /**
             * Maybe here we could try to recover the page
             */
            i_log_error ("Cannot get page %" PRpgno " because it is invalid in the database file: %s\n", pg, e->cause_msg);
            return ret;
          }

        // pgr is now loaded
        pgr->pin = 1;
        pgr->flags = 0;
        pgr->wsibling = -1;
        pgr->page.pg = pg;

        // Start page at access_bit = 1
        pf_set (pgr, PW_ACCESS);
        pf_set (pgr, PW_PRESENT);

        hdata_idx hd = (hdata_idx){ .key = pg, .value = p->clock };
        ht_insert_expect_idx (&p->pgno_to_value, hd);

        // Be nice to the next caller and iterate clock
        p->clock = (p->clock + 1) % MEMORY_PAGE_LEN;
        break;
      }
    }

  // Initialize page_h
  dest->pgr = pgr;
  dest->pgw = NULL;
  dest->mode = PHM_S;

  return SUCCESS;
}

err_t
pgr_get_unverified (page_h *dest, pgno pg, struct pager *p, error *e)
{
  DBG_ASSERT (page_h, dest);
  ASSERT (dest->mode == PHM_NONE);

  struct page_frame *pgr = NULL;

  err_t ret = SUCCESS;

  // Try to fetch from memory first
  hdata_idx data;
  switch (ht_get_idx (&p->pgno_to_value, &data, pg))
    {
    case HTAR_SUCCESS:
      {
        pgr = &p->pages[data.value];

        // TODO BLOCK ON X
        if (pgr->wsibling >= 0)
          {
            ASSERT (0 && "WOULD BLOCK ON X!");
          }

        pgr->pin++;
        break;
      }
    case HTAR_DOESNT_EXIST:
      {
        // Then load into a new spot
        ret = pgr_reserve_at_clock_thread_unsafe (p, e);
        if (ret)
          {
            return ret;
          }

        // Read in data to the current page
        pgr = &p->pages[p->clock];

        ret = fpgr_read (&p->fp, pgr->page.raw, pg, e);
        if (ret)
          {
            return ret;
          }

        // NOTE: Skip validation - this is the difference from pgr_get
        // Used during ARIES recovery to read pages that may be invalid

        // pgr is now loaded
        pgr->pin = 1;
        pgr->flags = 0;
        pgr->wsibling = -1;
        pgr->page.pg = pg;

        // Start page at access_bit = 1
        pf_set (pgr, PW_ACCESS);
        pf_set (pgr, PW_PRESENT);

        hdata_idx hd = (hdata_idx){ .key = pg, .value = p->clock };
        ht_insert_expect_idx (&p->pgno_to_value, hd);

        // Be nice to the next caller and iterate clock
        p->clock = (p->clock + 1) % MEMORY_PAGE_LEN;
        break;
      }
    }

  // Initialize page_h
  dest->pgr = pgr;
  dest->pgw = NULL;
  dest->mode = PHM_S;

  return SUCCESS;
}

err_t
pgr_make_writable_no_tx (struct pager *p, page_h *h, error *e)
{
  DBG_ASSERT (pager, p);
  ASSERT (h->mode == PHM_S);

  i_log_trace ("Pager making page: %" PRpgno " writable\n", page_h_pgno (h));

  err_t ret = SUCCESS;

  // Reserve room for writable page
  ret = pgr_reserve_at_clock_thread_unsafe (p, e);
  if (ret)
    {
      return ret;
    }

  struct page_frame *pgw = &p->pages[p->clock];

  // Mark page as dirty (will be added to DPT in pgr_save with proper LSN)
  bool was_dirty = pf_check (h->pgr, PW_DIRTY);
  if (!was_dirty)
    {
      pf_set (h->pgr, PW_DIRTY);
    }

  // Initialize w page
  pgw->pin = 1;
  pgw->wsibling = -1;
  pgw->flags = 0;
  pf_set (pgw, PW_PRESENT);
  pf_set (pgw, PW_X);
  i_memcpy (pgw->page.raw, h->pgr->page.raw, PAGE_SIZE);
  pgw->page.pg = h->pgr->page.pg;

  // Set page_h
  h->pgr->wsibling = p->clock;
  h->pgw = pgw;
  h->mode = PHM_X;
  h->pgw->pin++;

  // Increment clock to be nice to next consumers
  p->clock = (p->clock + 1) % MEMORY_PAGE_LEN;

  return SUCCESS;
}

err_t
pgr_make_writable (struct pager *p, struct txn *tx, page_h *h, error *e)
{
  ASSERT (tx);
  err_t_wrap (pgr_make_writable_no_tx (p, h, e), e);
  h->tx = tx;
  return SUCCESS;
}

err_t
pgr_maybe_make_writable (struct pager *p, struct txn *tx, page_h *cur, error *e)
{
  ASSERT (tx);
  if (cur->mode == PHM_S)
    {
      return pgr_make_writable (p, tx, cur, e);
    }
  return SUCCESS;
}

// No transaction version for recovery - no WAL logging
err_t
pgr_release_no_tx (struct pager *p, page_h *h, int flags, error *e)
{
  DBG_ASSERT (pager, p);
  ASSERT (h->mode == PHM_X || h->mode == PHM_S);

  i_log_trace ("Releasing (no tx) %" PRpgno "\n", page_h_pgno (h));

  if (h->mode == PHM_X)
    {
      // TODO - I think this is wrong
      // Add page to DPT if this is the first update (RecLSN = LSN of first update)
      if (!dpgt_exists (&p->dpt, page_h_pgno (h)))
        {
          if (dpgt_add (&p->dpt, page_h_pgno (h), (lsn)page_get_page_lsn (page_h_ro (h)), e))
            {
              return e->cause_code;
            }
        }

      // Save without WAL logging (recovery doesn't need WAL)
      ASSERTF (page_validate_for_db (page_h_w (h), flags, NULL) == SUCCESS,
               "%.*s\n", e->cmlen, e->cause_msg);

      i_memcpy (h->pgr->page.raw, h->pgw->page.raw, PAGE_SIZE);
      h->pgw->flags = 0;
      pf_clr (h->pgw, PW_PRESENT);
      h->pgr->wsibling = -1;
      h->pgw = NULL;
      h->mode = PHM_S;
    }

  err_t_wrap (page_validate_for_db (&h->pgr->page, flags, e), e);

  DBG_ASSERT (pager, p);

  h->pgr->pin--;
  h->pgr = NULL;
  h->mode = PHM_NONE;

  return SUCCESS;
}

// No transaction version for recovery
err_t
pgr_get_writable_no_tx (page_h *dest, int flags, pgno pg, struct pager *p, error *e)
{
  err_t_wrap (pgr_get (dest, flags, pg, p, e), e);
  err_t ret = pgr_make_writable_no_tx (p, dest, e);

  // Couldn't allocate writable page - release dest
  if (ret != SUCCESS)
    {
      pgr_release_no_tx (p, dest, flags, e);
    }

  return ret;
}

err_t
pgr_get_writable (page_h *dest, struct txn *tx, int flags, pgno pg, struct pager *p, error *e)
{
  err_t_wrap (pgr_get (dest, flags, pg, p, e), e);
  err_t ret = pgr_make_writable (p, tx, dest, e);

  // Couldn't allocate writable page - release dest
  if (ret != SUCCESS)
    {
      pgr_release_no_tx (p, dest, flags, e);
    }

  return ret;
}

err_t
pgr_save (struct pager *p, page_h *h, int flags, error *e)
{
  DBG_ASSERT (pager, p);
  ASSERT (h->mode == PHM_X);
  ASSERTF (page_validate_for_db (page_h_w (h), flags, NULL) == SUCCESS, "%.*s\n", e->cmlen, e->cause_msg);

  // Save log
  latch_lock (&h->tx->l);

  // Construct an update log record
  struct wal_update_write update = {
    .tid = h->tx->tid,
    .pg = page_h_pgno (h),
    .prev = h->tx->data.last_lsn,
    .undo = h->pgr->page.raw,
    .redo = h->pgw->page.raw,
  };

  // Append that update to the wal and get it's lsn
  slsn page_lsn = wal_append_update_log (&p->ww, update, e);
  if (page_lsn < 0)
    {
      latch_unlock (&h->tx->l);
      return e->cause_code;
    }

  // Update the page lsn
  page_set_page_lsn (page_h_w (h), (lsn)page_lsn);

  h->tx->data.last_lsn = page_lsn;
  h->tx->data.undo_next_lsn = page_lsn;

  // Add page to DPT if this is the first update (RecLSN = LSN of first update)
  if (!dpgt_exists (&p->dpt, page_h_pgno (h)))
    {
      if (dpgt_add (&p->dpt, page_h_pgno (h), (lsn)page_lsn, e))
        {
          latch_unlock (&h->tx->l);
          return e->cause_code;
        }
    }

  latch_unlock (&h->tx->l);

  i_memcpy (&h->pgr->page.raw, h->pgw->page.raw, PAGE_SIZE);
  h->pgw->flags = 0;
  pf_clr (h->pgw, PW_PRESENT);
  h->pgr->wsibling = -1;
  h->pgw = NULL;
  h->mode = PHM_S;

  return SUCCESS;
}

static void
pgr_cancel_w (struct pager *p, page_h *h)
{
  DBG_ASSERT (pager, p);
  ASSERT (h->mode == PHM_X);

  h->pgw->flags = 0;
  pf_clr (h->pgw, PW_PRESENT);
  h->pgr->wsibling = -1;
  h->pgw = NULL;
  h->mode = PHM_S;
}

err_t
pgr_new (page_h *dest, struct pager *p, struct txn *tx, enum page_type type, error *e)
{
  DBG_ASSERT (pager, p);
  DBG_ASSERT (page_h, dest);
  ASSERT (dest->mode == PHM_NONE);

  err_t ret = SUCCESS;
  page_h root_node = page_h_create ();

  // S(root)
  if (lockt_lock (p->lt, (struct lt_lock){ .type = LOCK_ROOT, .data = { 0 } }, LM_X, tx, e))
    {
      return e->cause_code;
    }

  /**
   * First, we'll check the tombstone to see if we can
   * dish out a new page that's already been deleted
   */
  err_t_wrap (pgr_get (&root_node, PG_ROOT_NODE, ROOT_PGNO, p, e), e);

  pgno ftpg = rn_get_first_tmbst (page_h_ro (&root_node));
  ret = pgr_make_writable (p, tx, &root_node, e);
  if (ret)
    {
      pgr_release_no_tx (p, &root_node, PG_ROOT_NODE, e);
      return ret;
    }

  // X(tombstone)
  if (lockt_lock (p->lt, (struct lt_lock){ .type = LOCK_TMBST, .data = { .tmbst_pg = ftpg } }, LM_X, tx, e))
    {
      return e->cause_code;
    }

  if (ftpg < fpgr_get_npages (&p->fp))
    {
      // Pop the first tombstone - this is our result page
      ret = pgr_get_writable (dest, tx, PG_TOMBSTONE, ftpg, p, e);
      if (ret)
        {
          pgr_cancel_w (p, &root_node);
          pgr_release_no_tx (p, &root_node, PG_ROOT_NODE, e);
          return ret;
        }
    }
  else
    {
      if ((ret = pgr_new_extend (dest, p, tx, e)))
        {
          pgr_cancel_w (p, &root_node);
          pgr_release_no_tx (p, &root_node, PG_ROOT_NODE, e);
          return ret;
        }
    }

  // Update root node's first tombstone link
  pgno ntbst = tmbst_get_next (page_h_ro (dest));
  rn_set_first_tmbst (page_h_w (&root_node), ntbst);
  ret = pgr_save (p, &root_node, PG_ROOT_NODE, e);

  if (ret)
    {
      pgr_cancel_w (p, dest);
      pgr_cancel_w (p, &root_node);
      pgr_release (p, &root_node, PG_ROOT_NODE, e);
      pgr_release (p, dest, type, e);
      return ret;
    }

  pgr_release (p, &root_node, PG_ROOT_NODE, e);
  page_init_empty (page_h_w (dest), type);

  return ret;
}

#ifndef NTEST
TEST (TT_UNIT, pgr_new_get_save)
{
  struct pgr_fixture f;
  page_h h = page_h_create ();
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);

  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  pgr_new (&h, f.p, &tx, PG_DATA_LIST, &f.e);
  test_assert_int_equal ((int)pgr_get_npages (f.p), 2);

  // Make it valid
  dl_set_used (page_h_w (&h), DL_DATA_SIZE);

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_release (f.p, &h, PG_DATA_LIST, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}
#endif

err_t
pgr_delete_and_release (struct pager *p, struct txn *tx, page_h *h, error *e)
{
  DBG_ASSERT (pager, p);

  err_t ret = SUCCESS;
  page_h root_node = page_h_create ();

  // Upgrade nodes to X
  err_t_wrap (pgr_get (&root_node, PG_ROOT_NODE, 0, p, e), e);
  ret = pgr_make_writable (p, tx, &root_node, e);
  if (ret)
    {
      return ret;
    }
  if (h->mode == PHM_S)
    {
      ret = pgr_make_writable (p, tx, h, e);
      if (ret)
        {
          pgr_cancel_w (p, &root_node);
          pgr_release (p, &root_node, PG_ROOT_NODE, e);
          return ret;
        }
    }

  pgno ftpg = rn_get_first_tmbst (page_h_ro (&root_node));
  page_init_empty (&h->pgw->page, PG_TOMBSTONE);
  tmbst_set_next (page_h_w (h), ftpg);
  ftpg = page_h_pgno (h);

  ret = pgr_release (p, h, PG_TOMBSTONE, e);

  /**
   * TODO - The following error handling logic doesn't look right
   */
  if (ret)
    {
      pgr_cancel_w (p, &root_node);
      pgr_release (p, &root_node, PG_ROOT_NODE, e);
      return ret;
    }

  rn_set_first_tmbst (page_h_w (&root_node), ftpg);
  ret = pgr_save (p, &root_node, PG_ROOT_NODE, e);
  if (ret)
    {
      pgr_cancel_w (p, &root_node);
      pgr_release (p, &root_node, PG_ROOT_NODE, e);
      return ret;
    }

  pgr_release (p, &root_node, PG_ROOT_NODE, e);

  return ret;
}

#ifndef NTEST
TEST (TT_UNIT, pgr_delete)
{
  struct pgr_fixture f;
  error *e = &f.e;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);

  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, e), e);

  page_h a = page_h_create ();
  page_h b = page_h_create ();
  page_h c = page_h_create ();
  page_h d = page_h_create ();

  test_err_t_wrap (pgr_new (&a, f.p, &tx, PG_DATA_LIST, e), e);
  test_err_t_wrap (pgr_new (&b, f.p, &tx, PG_DATA_LIST, e), e);
  test_err_t_wrap (pgr_new (&c, f.p, &tx, PG_DATA_LIST, e), e);
  test_err_t_wrap (pgr_new (&d, f.p, &tx, PG_DATA_LIST, e), e);

  pgno apg = page_h_pgno (&a);
  pgno bpg = page_h_pgno (&b);
  pgno cpg = page_h_pgno (&c);
  pgno dpg = page_h_pgno (&d);

  test_err_t_wrap (pgr_delete_and_release (f.p, &tx, &a, e), e);
  test_err_t_wrap (pgr_delete_and_release (f.p, &tx, &b, e), e);
  test_err_t_wrap (pgr_delete_and_release (f.p, &tx, &c, e), e);
  test_err_t_wrap (pgr_delete_and_release (f.p, &tx, &d, e), e);

  test_err_t_wrap (pgr_new (&a, f.p, &tx, PG_DATA_LIST, e), e);
  test_err_t_wrap (pgr_new (&b, f.p, &tx, PG_DATA_LIST, e), e);
  test_err_t_wrap (pgr_new (&c, f.p, &tx, PG_DATA_LIST, e), e);
  test_err_t_wrap (pgr_new (&d, f.p, &tx, PG_DATA_LIST, e), e);

  test_assert_equal (page_h_pgno (&a), dpg);
  test_assert_equal (page_h_pgno (&b), cpg);
  test_assert_equal (page_h_pgno (&c), bpg);
  test_assert_equal (page_h_pgno (&d), apg);

  test_err_t_wrap (pgr_delete_and_release (f.p, &tx, &a, e), e);
  test_err_t_wrap (pgr_delete_and_release (f.p, &tx, &b, e), e);
  test_err_t_wrap (pgr_delete_and_release (f.p, &tx, &c, e), e);
  test_err_t_wrap (pgr_delete_and_release (f.p, &tx, &d, e), e);

  test_err_t_wrap (pgr_new (&a, f.p, &tx, PG_DATA_LIST, e), e);
  test_err_t_wrap (pgr_new (&b, f.p, &tx, PG_DATA_LIST, e), e);
  test_err_t_wrap (pgr_new (&c, f.p, &tx, PG_DATA_LIST, e), e);
  test_err_t_wrap (pgr_new (&d, f.p, &tx, PG_DATA_LIST, e), e);

  test_assert_equal (page_h_pgno (&a), apg);
  test_assert_equal (page_h_pgno (&b), bpg);
  test_assert_equal (page_h_pgno (&c), cpg);
  test_assert_equal (page_h_pgno (&d), dpg);

  dl_set_used (page_h_w (&a), DL_DATA_SIZE);
  dl_set_used (page_h_w (&b), DL_DATA_SIZE);
  dl_set_used (page_h_w (&c), DL_DATA_SIZE);
  dl_set_used (page_h_w (&d), DL_DATA_SIZE);

  test_err_t_wrap (pgr_release (f.p, &a, PG_DATA_LIST, e), e);
  test_err_t_wrap (pgr_delete_and_release (f.p, &tx, &b, e), e);
  test_err_t_wrap (pgr_delete_and_release (f.p, &tx, &c, e), e);
  test_err_t_wrap (pgr_release (f.p, &d, PG_DATA_LIST, e), e);

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}
#endif

err_t
pgr_release_if_exists (struct pager *p, page_h *h, int flags, error *e)
{
  if (h->mode != PHM_NONE)
    {
      return pgr_release (p, h, flags, e);
    }
  return SUCCESS;
}

err_t
pgr_release (struct pager *p, page_h *h, int flags, error *e)
{
  DBG_ASSERT (pager, p);
  ASSERT (h->mode == PHM_X || h->mode == PHM_S);
  ASSERT (pf_check (h->pgr, PW_PRESENT));

  i_log_trace ("Releasing %" PRpgno "\n", page_h_pgno (h));

  if (h->mode == PHM_X)
    {
      err_t_wrap (pgr_save (p, h, flags, e), e);
    }

  err_t_wrap (page_validate_for_db (&h->pgr->page, flags, e), e);

  DBG_ASSERT (pager, p);

  h->pgr->pin--;
  h->pgr = NULL;
  h->mode = PHM_NONE;

  return SUCCESS;
}

err_t
pgr_flush_wall (struct pager *p, error *e)
{
  return wal_flush_all (&p->ww, e);
}

#ifndef NTEST

TEST (TT_UNIT, pager_fill_ht)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);

  struct txn tx;
  pgr_begin_txn (&tx, f.p, &f.e);

  page_h pgs[MEMORY_PAGE_LEN];
  page_h bad = page_h_create ();

  {
    // Fill up - there is already one page in the pool, the root
    u32 i = 0;
    for (; i < MEMORY_PAGE_LEN / 2 - 1; ++i)
      {
        pgs[i] = page_h_create ();
        test_err_t_wrap (pgr_new (&pgs[i], f.p, &tx, PG_DATA_LIST, &f.e), &f.e);
        test_assert_equal (pgs[i].mode, PHM_X);
      }

    // Next page will be full
    test_err_t_check (pgr_new (&bad, f.p, &tx, PG_DATA_LIST, &f.e), ERR_PAGER_FULL, &f.e);

    // Release them all
    for (i = 0; i < MEMORY_PAGE_LEN / 2 - 1; ++i)
      {
        dl_set_used (page_h_w (&pgs[i]), DL_DATA_SIZE);
        test_err_t_wrap (pgr_release (f.p, &pgs[i], PG_DATA_LIST, &f.e), &f.e);
      }
  }

  // Repeat above
  {
    // Fill half way up - good
    for (u32 i = 0; i < MEMORY_PAGE_LEN / 2 - 1; ++i)
      {
        test_err_t_wrap (pgr_new (&pgs[i], f.p, &tx, PG_DATA_LIST, &f.e), &f.e);
        test_assert_equal (pgs[i].mode, PHM_X);
      }

    // Next page will be full
    test_err_t_check (pgr_new (&bad, f.p, &tx, PG_DATA_LIST, &f.e), ERR_PAGER_FULL, &f.e);

    // Release them all
    for (u32 i = 0; i < MEMORY_PAGE_LEN / 2 - 1; ++i)
      {
        dl_set_used (page_h_w (&pgs[i]), DL_DATA_SIZE);
        test_err_t_wrap (pgr_release (f.p, &pgs[i], PG_DATA_LIST, &f.e), &f.e);
      }
  }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}
#endif

#ifndef NTEST
TEST (TT_UNIT, wal_int)
{
  struct pgr_fixture f;
  page_h h = page_h_create ();
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);

  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  test_err_t_wrap (pgr_new (&h, f.p, &tx, PG_DATA_LIST, &f.e), &f.e);

  dl_set_used (page_h_w (&h), DL_DATA_SIZE);
  test_err_t_wrap (pgr_release (f.p, &h, PG_DATA_LIST, &f.e), &f.e);

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}
#endif

void
i_log_page_table (int log_level, struct pager *p)
{
  DBG_ASSERT (pager, p);
  i_log (log_level, "Page Table:\n");
  for (u32 i = 0; i < MEMORY_PAGE_LEN; ++i)
    {
      struct page_frame *mp = &p->pages[i];
      if (pf_check (mp, PW_PRESENT))
        {
          i_printf (log_level, "%u |(PAGE)    pg: %" PRpgno " pin: %d ax: %d drt: %d prsn: %d sib: %d type: %d|\n",
                    i,
                    mp->page.pg,
                    mp->pin,
                    pf_check (mp, PW_ACCESS),
                    pf_check (mp, PW_DIRTY),
                    pf_check (mp, PW_PRESENT),
                    mp->wsibling,
                    page_get_type (&mp->page));
        }
      else
        {
          i_printf (log_level, "%u | |\n", i);
        }
    }
  i_log_dpgt (log_level, &p->dpt);
  i_log_txnt (log_level, &p->tnxt);
}

#ifndef NTEST

/**
TEST (TT_UNIT, pager_wal_entries_correct)
{
  error e = error_create ();
  test_err_t_wrap (i_remove_quiet ("test.db", &e), &e);
  test_err_t_wrap (i_remove_quiet ("test.wal", &e), &e);

  decl_rand_buffer (_pg2, u8, PAGE_SIZE);
  decl_rand_buffer (_pg22, u8, PAGE_SIZE);
  decl_rand_buffer (_pg3, u8, PAGE_SIZE);

  // BEGIN (tid1)
  // UPDATE (tid1)
  // COMMIT (tid1)
  // END (tid1)
  struct pager *p = pgr_open ("test.db", "test.wal", &e);
  test_fail_if_null (p);

  // BEGIN (tid2)
  struct txn tx2;
  test_err_t_wrap (pgr_begin_txn (&tx2, p, &e), &e);

  page_h pg2 = page_h_create ();
  pgr_new (&pg2, p, &tx2, PG_DATA_LIST, &e);
  dl_set_data (
      page_h_w (&pg2),
      (struct dl_data){
          .data = &_pg2[PG_COMMN_END],
          .blen = DL_DATA_SIZE,
      });

  i_memcpy (_pg2, page_h_w (&pg2)->raw, PAGE_SIZE);

  // UPDATE (tid2)
  pgr_save (p, &pg2, PG_DATA_LIST, &e);

  // BEGIN (tid3)
  struct txn tx3;
  test_err_t_wrap (pgr_begin_txn (&tx3, p, &e), &e);

  page_h pg3 = page_h_create ();
  pgr_new (&pg3, p, &tx3, PG_DATA_LIST, &e);
  dl_set_data (
      page_h_w (&pg3),
      (struct dl_data){
          .data = &_pg3[DL_DATA_SIZE],
          .blen = DL_DATA_SIZE,
      });

  i_memcpy (_pg3, page_h_w (&pg3)->raw, PAGE_SIZE);

  // UPDATE (tid3)
  pgr_release (p, &pg3, PG_DATA_LIST, &e);

  // COMMIT (tid3)
  // END (tid3)
  pgr_commit (p, &tx3, &e);

  pgr_make_writable (p, &tx2, &pg2, &e);
  dl_set_data (
      page_h_w (&pg2),
      (struct dl_data){
          .data = &_pg22[DL_DATA_SIZE],
          .blen = DL_DATA_SIZE,
      });

  i_memcpy (_pg22, page_h_w (&pg2)->raw, PAGE_SIZE);

  // UPDATE (tid2)
  pgr_release (p, &pg2, PG_DATA_LIST, &e);

  // COMMIT (tid2)
  // END (tid2)
  pgr_commit (p, &tx2, &e);

  test_err_t_wrap (pgr_close (p, &e), &e);

  struct wal_rec_hdr_read expected[] = {
    {
        .type = WL_BEGIN,
        .begin = {
            .tid = 1,
        },
    },
    {
        .type = WL_UPDATE,
        .update = {
            .tid = 1,
            .pg = 0,
            .prev = PGNO_NULL,
            .undo = NULL,
            .redo = NULL,
        },
    },
    {
        .type = WL_COMMIT,
        .commit = {
            .tid = 1,
            .prev = 0,
        },
    },
    {
        .type = WL_END,
        .end = {
            .tid = 1,
            .prev = 4112,
        },
    },
    {
        .type = WL_BEGIN,
        .begin = {
            .tid = 2,
        },
    },
    {
        .type = WL_UPDATE,
        .update = {
            .tid = 2,
            .pg = 1,
            .prev = PGNO_NULL,
            .undo = NULL,
            .redo = _pg2,
        },
    },
    {
        .type = WL_BEGIN,
        .begin = {
            .tid = 3,
        },
    },
    {
        .type = WL_UPDATE,
        .update = {
            .tid = 3,
            .pg = 2,
            .prev = PGNO_NULL,
            .undo = NULL,
            .redo = _pg3,
        },
    },
    {
        .type = WL_COMMIT,
        .commit = {
            .tid = 3,
            .prev = 8266,
        },
    },
    {
        .type = WL_END,
        .end = {
            .tid = 3,
            .prev = 12378,
        },
    },
    {
        .type = WL_UPDATE,
        .update = {
            .tid = 2,
            .pg = 1,
            .prev = 4167,
            .undo = _pg2,
            .redo = _pg22,
        },
    },
    {
        .type = WL_COMMIT,
        .commit = {
            .tid = 2,
            .prev = 4154,
        },
    },
    {
        .type = WL_END,
        .end = {
            .tid = 2,
            .prev = 16519,
        },
    },
  };

  struct wal ww;
  test_err_t_wrap (wal_open (&ww, "test.wal", &e), &e);

  // READ
  {
    // Read them from the start
    for (u32 i = 0; i < arrlen (expected); i++)
      {
        lsn read_lsn;
        struct wal_rec_hdr *next = wal_read_next (&ww, &read_lsn, &e);
        test_fail_if_null (next);
        i_log_wal_rec_hdr (LOG_INFO, next);
        i_log_wal_rec_hdr (LOG_INFO, &expected[i]);
        test_assert (wal_rec_hdr_equal (next, &expected[i]));
      }
  }

  test_err_t_wrap (wal_close (&ww, &e), &e);
}
*/

/**
TEST (TT_UNIT, pager_wal_entries_on_crash)
{
  error e = error_create ();
  test_err_t_wrap (i_remove_quiet ("test.db", &e), &e);
  test_err_t_wrap (i_remove_quiet ("test.wal", &e), &e);

  decl_rand_buffer (_pg2, u8, PAGE_SIZE);
  decl_rand_buffer (_pg22, u8, PAGE_SIZE);
  decl_rand_buffer (_pg3, u8, PAGE_SIZE);

  // BEGIN (tid1)
  // UPDATE (tid1)
  // COMMIT (tid1)
  // END (tid1)
  struct pager *p = pgr_open ("test.db", "test.wal", &e);
  test_fail_if_null (p);

  // BEGIN (tid2)
  struct txn tx2;
  test_err_t_wrap (pgr_begin_txn (&tx2, p, &e), &e);

  page_h pg2 = page_h_create ();
  pgr_new (&pg2, p, &tx2, PG_DATA_LIST, &e);
  dl_set_data (
      page_h_w (&pg2),
      (struct dl_data){
          .data = &_pg2[PG_COMMN_END],
          .blen = DL_DATA_SIZE,
      });

  i_memcpy (_pg2, page_h_w (&pg2)->raw, PAGE_SIZE);

  // UPDATE (tid2)
  pgr_save (p, &pg2, PG_DATA_LIST, &e);

  // BEGIN (tid3)
  struct txn tx3;
  test_err_t_wrap (pgr_begin_txn (&tx3, p, &e), &e);

  page_h pg3 = page_h_create ();
  pgr_new (&pg3, p, &tx3, PG_DATA_LIST, &e);
  dl_set_data (
      page_h_w (&pg3),
      (struct dl_data){
          .data = &_pg3[DL_DATA_SIZE],
          .blen = DL_DATA_SIZE,
      });

  i_memcpy (_pg3, page_h_w (&pg3)->raw, PAGE_SIZE);

  // UPDATE (tid3)
  pgr_release (p, &pg3, PG_DATA_LIST, &e);

  // COMMIT (tid3)
  // END (tid3)
  pgr_commit (p, &tx3, &e);

  pgr_make_writable (p, &tx2, &pg2, &e);
  dl_set_data (
      page_h_w (&pg2),
      (struct dl_data){
          .data = &_pg22[DL_DATA_SIZE],
          .blen = DL_DATA_SIZE,
      });

  i_memcpy (_pg22, page_h_w (&pg2)->raw, PAGE_SIZE);

  // UPDATE (tid2)
  pgr_release (p, &pg2, PG_DATA_LIST, &e);

  // COMMIT (tid2)
  // END (tid2)
  pgr_commit (p, &tx2, &e);

  test_err_t_wrap (pgr_close (p, &e), &e);

  struct wal_rec_hdr_read expected[] = {
    {
        .type = WL_BEGIN,
        .begin = {
            .tid = 1,
        },
    },
    {
        .type = WL_UPDATE,
        .update = {
            .tid = 1,
            .pg = 0,
            .prev = PGNO_NULL,
            .undo = NULL,
            .redo = NULL,
        },
    },
    {
        .type = WL_COMMIT,
        .commit = {
            .tid = 1,
            .prev = 0,
        },
    },
    {
        .type = WL_END,
        .end = {
            .tid = 1,
            .prev = 4112,
        },
    },
    {
        .type = WL_BEGIN,
        .begin = {
            .tid = 2,
        },
    },
    {
        .type = WL_UPDATE,
        .update = {
            .tid = 2,
            .pg = 1,
            .prev = PGNO_NULL,
            .undo = NULL,
            .redo = _pg2,
        },
    },
    {
        .type = WL_BEGIN,
        .begin = {
            .tid = 3,
        },
    },
    {
        .type = WL_UPDATE,
        .update = {
            .tid = 3,
            .pg = 2,
            .prev = PGNO_NULL,
            .undo = NULL,
            .redo = _pg3,
        },
    },
    {
        .type = WL_COMMIT,
        .commit = {
            .tid = 3,
            .prev = 8266,
        },
    },
    {
        .type = WL_END,
        .end = {
            .tid = 3,
            .prev = 12378,
        },
    },
    {
        .type = WL_UPDATE,
        .update = {
            .tid = 2,
            .pg = 1,
            .prev = 4167,
            .undo = _pg2,
            .redo = _pg22,
        },
    },
    {
        .type = WL_COMMIT,
        .commit = {
            .tid = 2,
            .prev = 4154,
        },
    },
    {
        .type = WL_END,
        .end = {
            .tid = 2,
            .prev = 16519,
        },
    },
  };
}
*/

#endif

#ifndef NTEST
err_t
pgr_crash (struct pager *p, error *e)
{
  if (p->wal_enabled)
    {
      wal_crash (&p->ww, e);
    }
  fpgr_crash (&p->fp, e);

  txnt_crash (&p->tnxt);
  dpgt_crash (&p->dpt);

  i_free (p);

  return e->cause_code;
}

#endif
