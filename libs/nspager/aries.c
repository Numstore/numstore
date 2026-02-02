#include "numstore/core/dbl_buffer.h"
#include "numstore/core/error.h"
#include "numstore/core/slab_alloc.h"
#include "numstore/pager/txn.h"
#include <aries.h>

#include <_pager.h>

err_t
aries_ctx_create (struct aries_ctx *dest, lsn master_lsn, error *e)
{
  bool txt_open = false;
  bool dpt_open = false;
  bool txn_ptrs_open = false;
  dest->master_lsn = master_lsn;
  dest->max_tid = 0;
  slab_alloc_init (&dest->alloc, sizeof (struct txn), 1000);

  if (txnt_open (&dest->txt, e))
    {
      goto failed;
    }
  txt_open = true;

  if (dpgt_open (&dest->dpt, e))
    {
      goto failed;
    }
  dpt_open = true;

  err_t_wrap_goto (dblb_create (&dest->txn_ptrs, sizeof (struct txn *), 100, e), failed, e);
  txn_ptrs_open = true;

  return SUCCESS;

failed:

  slab_alloc_destroy (&dest->alloc);

  if (txt_open)
    {
      txnt_close (&dest->txt);
    }
  if (txn_ptrs_open)
    {
      dblb_free (&dest->txn_ptrs);
    }
  if (dpt_open)
    {
      dpgt_close (&dest->dpt);
    }

  return e->cause_code;
}

void
aries_ctx_free (struct aries_ctx *ctx)
{
  ASSERT (ctx);
  slab_alloc_destroy (&ctx->alloc);
  txnt_close (&ctx->txt);
  dpgt_close (&ctx->dpt);
  dblb_free (&ctx->txn_ptrs);
}

//////////////////////////////////////////////////////////////////////////////////
/////////////////////// RESTART (Figure 9)

err_t
pgr_restart (struct pager *p, struct aries_ctx *ctx, error *e)
{
  i_log_info ("Pgr Restart. Master Lsn: %" PRlsn "\n", ctx->master_lsn);

  err_t ret = SUCCESS;
  p->restarting = true;

  // ANALYSIS
  if (pgr_restart_analysis (p, ctx, e))
    {
      goto theend;
    }

  // REDO
  if (pgr_restart_redo (p, ctx, e))
    {
      goto theend;
    }

  // UNDO
  if (pgr_restart_undo (p, ctx, e))
    {
      goto theend;
    }

theend:
  aries_ctx_free (ctx);
  p->restarting = false;

  return e->cause_code;
}

//////////////////////////////////////////////////////////////////////////////////
/////////////////////// ROLLBACK (Figure 8)

err_t
pgr_rollback (struct pager *p, struct txn *tx, lsn save_lsn, error *e)
{
  latch_lock (&tx->l);

  struct wal_rec_hdr_read *log_rec = NULL;
  struct wal_clr_write clr;
  slsn clr_lsn;
  page_h ph = page_h_create ();

  // UndoNxt := Trans_Table[TransId].UndoNxtLSN
  lsn undo_nxt_lsn = tx->data.undo_next_lsn;
  txid tid = tx->tid;

  // WHILE SaveSN < UndoNxt DO:
  while (save_lsn < undo_nxt_lsn)
    {
      // LogRec := Log_Read(UndoNxt)
      if ((log_rec = wal_read_entry (&p->ww, undo_nxt_lsn, e)) == NULL)
        {
          return e->cause_code;
        }

      if (log_rec->type == WL_EOF)
        {
        }

      // SELECT (LogRec.Type)
      switch (log_rec->type)
        {

          // WHEN('update') DO;
        case WL_UPDATE:
          {
            // Save values that might be overwritten when we write the CLR
            pgno pg = log_rec->update.pg;
            lsn prev_lsn = log_rec->update.prev;
            txid update_tid = log_rec->update.tid;

            // IF LogRec is undoable THEN DO
            {
              // Page := fix&latch(LogRec.PageID, 'X')
              err_t_wrap (pgr_get_writable_no_tx (&ph, PG_ANY, pg, p, e), e);

              // Undo_Update(Page, LogRec)
              i_memcpy (ph.pgw->page.raw, log_rec->update.undo, PAGE_SIZE);

              // Log_Write
              clr_lsn = wal_append_clr_log (
                  &p->ww,
                  (struct wal_clr_write){
                      .tid = update_tid,            // LogRec.TransID
                      .prev = tx->data.last_lsn,    // Trans_Table[TransID].LastLSN
                      .pg = pg,                     // LogRec.PageID
                      .undo_next = prev_lsn,        // LogRec.PrevLSN
                      .redo = log_rec->update.undo, // Data
                  },
                  e);

              if (clr_lsn < 0)
                {
                  pgr_release_no_tx (p, &ph, PG_ANY, NULL);
                  return e->cause_code;
                }

              // Page.LSN = LgLSN
              page_set_page_lsn (page_h_w (&ph), clr_lsn);

              // Trans_Table[TransID].LastLSN = LgLSN
              tx->data.last_lsn = clr_lsn;

              // unfix&unlatch(Page)
              err_t_wrap (pgr_release_no_tx (p, &ph, PG_ANY, e), e);

            } // END

            // UndoNxt := LogRec.PrevLSN
            undo_nxt_lsn = prev_lsn;

            break;
          }

        case WL_CLR:
          {
            // UndoNxt := LogRec.UndoNxtLSN
            undo_nxt_lsn = log_rec->clr.undo_next;
            break;
          }

        case WL_BEGIN:
          {
            undo_nxt_lsn = 0; // Done
            break;
          }
        case WL_COMMIT:
          {
            return error_causef (
                e, ERR_CORRUPT,
                "Got a commit record in rollback transaction chain. lsn: %" PRlsn, undo_nxt_lsn);
          }
        case WL_END:
          {
            return error_causef (
                e, ERR_CORRUPT,
                "Got a end record in rollback transaction chain. lsn: %" PRlsn, undo_nxt_lsn);
          }
        case WL_CKPT_BEGIN:
          {
            return error_causef (
                e, ERR_CORRUPT,
                "Got a checkpoint begin in rollback transaction chain. lsn: %" PRlsn, undo_nxt_lsn);
          }
        case WL_CKPT_END:
          {
            txnt_close (&log_rec->ckpt_end.att);
            dpgt_close (&log_rec->ckpt_end.dpt);
            if (log_rec->ckpt_end.txn_bank)
              {
                i_free (log_rec->ckpt_end.txn_bank);
              }
            return error_causef (
                e, ERR_CORRUPT,
                "Got a checkpoint end in rollback transaction chain. lsn: %" PRlsn, undo_nxt_lsn);
          }

        case WL_EOF:
          {
            goto theend;
          }
        }

      // Trans_Table[TransID].UndoNxtLSN := UndoNxt
      tx->data.undo_next_lsn = undo_nxt_lsn;
    }

theend:

  latch_unlock (&tx->l);

  return SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////
/////////////////////// ANALYSIS (Figure 10)

struct wal_txnt_error
{
  const struct txn_table *t;
  struct wal *w;
  error *e;
};

static void
aries_append_end_records (struct txn *tx, void *ctx)
{
  struct wal_txnt_error *_ctx = ctx;
  if (_ctx->e->cause_code)
    {
      return;
    }

  // Reached the bottom of the chain
  bool base = (((tx->data.state == TX_CANDIDATE_FOR_UNDO) && tx->data.undo_next_lsn == 0));

  // Finished commit but no End Record
  bool committed = tx->data.state == TX_COMMITTED;

  if (base || committed)
    {
      // Append an end log
      slsn l = wal_append_end_log (_ctx->w, tx->tid, tx->data.last_lsn, _ctx->e);
      if (l < 0)
        {
          return;
        }

      // State -> Done
      txn_update_state (tx, TX_DONE);
    }
}

/**
 * Loops through open transactions. If they are done
 * (committed or finished rollback with no END)
 * appends an END record then removes them from the
 * active transaction table
 *
 * Remember that analysis doesn't do any rolling back
 * or forward movement, so these are just transactions
 * left in a state without an END record from the
 * previous time the db was open
 */
static err_t
pgr_analysis_finish_some_open_txn_ptrs (struct pager *p, struct aries_ctx *ctx, error *e)
{
  // Loop through and append end records
  struct wal_txnt_error _ctx = {
    .t = &ctx->txt,
    .w = &p->ww,
    .e = e,
  };

  txnt_foreach (&ctx->txt, aries_append_end_records, &_ctx);

  if (e->cause_code)
    {
      return e->cause_code;
    }

  // Remove them all from the table
  struct txn *txn_ptrs = ctx->txn_ptrs.data;
  for (u32 i = 0; i < ctx->txn_ptrs.nelem; ++i)
    {
      if (txn_ptrs[i].data.state == TX_DONE)
        {
          err_t_wrap (txnt_remove_txn_expect (&ctx->txt, &txn_ptrs[i], e), e);
        }
    }

  return SUCCESS;
}

err_t
pgr_restart_analysis (struct pager *p, struct aries_ctx *ctx, error *e)
{
  i_log_info ("Pgr Restart Analysis\n");

  // Open_Log_Scan
  lsn read_lsn = 0;
  struct wal_rec_hdr_read *log_rec = NULL;

  // If we have a master record (checkpoint), start from there
  if (ctx->master_lsn > 0)
    {
      // Master_Rec := Read_Disk(Master_Addr)
      // LogRec := Next_Log() // Read in the begin chkpt record
      struct wal_rec_hdr_read *master_rec = wal_read_entry (&p->ww, ctx->master_lsn, e);
      if (master_rec == NULL)
        {
          goto theend;
        }
      if (master_rec->type != WL_CKPT_BEGIN)
        {
          return error_causef (e, ERR_CORRUPT, "Master LSN points to a non begin checkpoint");
        }

      // LogRec := Next_Log() // Read log record following Begin_Chkpt
      log_rec = wal_read_next (&p->ww, &read_lsn, e);
      if (log_rec == NULL)
        {
          goto theend;
        }
    }

  // If no checkpoint or checkpoint theend, start from beginning
  if (ctx->master_lsn == 0)
    {
      log_rec = wal_read_next (&p->ww, &read_lsn, e);
      if (log_rec == NULL)
        {
          return e->cause_code;
        }
    }

  ctx->redo_lsn = 0;

  while (log_rec->type != WL_EOF)
    {
      update_max_txid (&ctx->max_tid, wrh_get_tid (log_rec));

      // IF trans related record & LogRec.TransID NOT in TRANS_TABLE THEN
      // insert(LOGRec.TransID, 'U', LogRec.LSN, LogRec.PrevLSN)
      stxid tid = wrh_get_tid (log_rec);
      struct txn *tx = NULL;

      if (tid != -1)
        {
          slsn prev_lsn = wrh_get_prev_lsn (log_rec);

          if (!txnt_get (&tx, &ctx->txt, tid))
            {
              // Create a new transaction object
              tx = slab_alloc_alloc (&ctx->alloc, e);
              if (tx == NULL)
                {
                  return e->cause_code;
                }
              if (dblb_append (&ctx->txn_ptrs, &tx, 1, e))
                {
                  return e->cause_code;
                }

              // Fetch the previous lsn
              ASSERT (prev_lsn >= 0);

              txn_init (tx, tid, (struct txn_data){
                                     .state = TX_CANDIDATE_FOR_UNDO,
                                     .last_lsn = read_lsn,
                                     .undo_next_lsn = prev_lsn,
                                 });

              // Insert this transaction
              err_t_wrap (txnt_insert_txn_if_not_exists (&ctx->txt, tx, e), e);
            }
          else
            {
              txn_update (tx, TX_CANDIDATE_FOR_UNDO, read_lsn, prev_lsn);
            }
        }

      switch (log_rec->type)
        {
        case WL_UPDATE:
          {
            // Trans_Table[LogRec.TransID].LastLSN := LogRec.LSN
            // Trans_Table[LogRec.TransID].UndoNxtLSN := LogRec.LSN
            txn_update_last_undo (tx, read_lsn, read_lsn);

            // IF LogRec.PageID not in Dirty_Page_Table THEN
            //   Dirty_Page_Table[LogRec.PageID].RecLSN := LogRec.LSN
            pgno pg = log_rec->update.pg;
            if (!dpgt_exists (&ctx->dpt, pg))
              {
                err_t_wrap_goto (dpgt_add (&ctx->dpt, pg, read_lsn, e), theend, e);
              }

            break;
          }
        case WL_CLR:
          {
            // Trans_Table[LogRec.TransID].LastLSN := LogRec.LSN
            // Trans_Table[LogRec.TransID].UndoNxtLSN := LogRec.UndoNxtLSN
            txn_update_last_undo (tx, read_lsn, log_rec->clr.undo_next);

            break;
          }
        case WL_CKPT_BEGIN:
          {
            // Skip this pointless checkpoint
            break;
          }
        case WL_CKPT_END:
          {
            // FOR each entry in LogRec.Tran_Table
            if (txnt_merge_into (&ctx->txt, &log_rec->ckpt_end.att, &ctx->txn_ptrs, &ctx->alloc, e))
              {
                goto theend;
              }

            // FOR each entry in LogRec.Dirty_PageLst
            u32 ckpt_dpt_count = dpgt_get_size (&log_rec->ckpt_end.dpt);

            if (dpgt_merge_into (&ctx->dpt, &log_rec->ckpt_end.dpt, e))
              {
                goto theend;
              }

            dpgt_close (&log_rec->ckpt_end.dpt);
            txnt_close (&log_rec->ckpt_end.att);
            if (log_rec->ckpt_end.txn_bank)
              {
                i_free (log_rec->ckpt_end.txn_bank);
              }

            break;
          }
        case WL_COMMIT:
          {
            // Update transaction state and last LSN in the ATT
            txn_update_last_state (tx, read_lsn, TX_COMMITTED);
            break;
          }
        case WL_BEGIN:
          {
            break;
          }
        case WL_END:
          {
            err_t_wrap (txnt_remove_txn_expect (&ctx->txt, tx, e), e);
            break;
          }
        case WL_EOF:
          {
            UNREACHABLE ();
          }
        }

      log_rec = wal_read_next (&p->ww, &read_lsn, e);
      if (log_rec == NULL)
        {
          return e->cause_code;
        }
    }

theend:
  // FOR EACH Trans_Table entry with (State == 'U') & (UndoNxtLSN = 0) DO
  //    write end record and remove entry from Trans_Table
  err_t_wrap (pgr_analysis_finish_some_open_txn_ptrs (p, ctx, e), e);

  ctx->redo_lsn = dpgt_min_rec_lsn (&ctx->dpt);

  return e->cause_code;
}

//////////////////////////////////////////////////////////////////////////////////
/////////////////////// REDO (FIGURE 11)

err_t
pgr_restart_redo (struct pager *p, struct aries_ctx *ctx, error *e)
{
  i_log_info ("Pgr Restart Redo\n");

  if (ctx->redo_lsn == 0)
    {
      return SUCCESS;
    }

  // Open_Log_Scan(RedoLSN)
  // LogRec = Next_Log()
  struct wal_rec_hdr_read *log_rec = wal_read_entry (&p->ww, ctx->redo_lsn, e);
  if (log_rec == NULL)
    {
      return e->cause_code;
    }

  // While NOT(End_Of_Log) DO;
  while (log_rec->type != WL_EOF)
    {
      update_max_txid (&ctx->max_tid, wrh_get_tid (log_rec));

      switch (log_rec->type)
        {
        case WL_UPDATE:
          {
            lsn rec_lsn;
            bool in_dpgt = dpgt_get (&rec_lsn, &ctx->dpt, log_rec->update.pg);

            if (in_dpgt && ctx->redo_lsn >= rec_lsn)
              {
                // fix&latch(LogRec.PageID, 'X')
                page_h ph = page_h_create ();
                err_t_wrap (pgr_get_unverified (&ph, log_rec->update.pg, p, e), e);
                err_t_wrap (pgr_make_writable_no_tx (p, &ph, e), e);

                // IF Page.LSN < LogRec.LSN
                lsn page_lsn = page_get_page_lsn (page_h_ro (&ph));
                if (page_lsn < ctx->redo_lsn)
                  {
                    // Redo_Update(Page, LogRec)
                    i_memcpy (page_h_w (&ph)->raw, log_rec->update.redo, PAGE_SIZE);
                    page_set_page_lsn (page_h_w (&ph), ctx->redo_lsn);
                  }
                else
                  {
                    dpgt_update (&ctx->dpt, log_rec->update.pg, page_lsn + 1);
                  }

                // unfix&unlatch(page)
                err_t_wrap (pgr_release_no_tx (p, &ph, PG_ANY, e), e);
              }
            break;
          }
        case WL_CLR:
          {
            lsn rec_lsn;
            bool in_dpgt = dpgt_get (&rec_lsn, &ctx->dpt, log_rec->clr.pg);

            if (in_dpgt && ctx->redo_lsn >= rec_lsn)
              {
                // fix&latch(LogRec.PageID, 'X')
                page_h ph = page_h_create ();
                err_t_wrap (pgr_get_unverified (&ph, log_rec->clr.pg, p, e), e);
                err_t_wrap (pgr_make_writable_no_tx (p, &ph, e), e);

                // IF Page.LSN < LogRec.LSN
                lsn page_lsn = page_get_page_lsn (page_h_ro (&ph));
                if (page_lsn < ctx->redo_lsn)
                  {
                    // Redo_Update(Page, LogRec)
                    i_memcpy (page_h_w (&ph)->raw, log_rec->clr.redo, PAGE_SIZE);
                    page_set_page_lsn (page_h_w (&ph), ctx->redo_lsn);
                  }
                else
                  {
                    dpgt_update (&ctx->dpt, log_rec->clr.pg, page_lsn + 1);
                  }

                // unfix&unlatch(page)
                pgr_release_no_tx (p, &ph, PG_ANY, NULL);
              }
            break;
          }
        case WL_CKPT_END:
          {
            // Checkpoint end records during redo need their ATT/DPT freed
            // since we don't use them in redo phase (only in analysis)
            txnt_close (&log_rec->ckpt_end.att);
            dpgt_close (&log_rec->ckpt_end.dpt);
            break;
          }
        default:
          {
            /* Do nothing */
            break;
          }
        }

      // Read next log record
      log_rec = wal_read_next (&p->ww, &ctx->redo_lsn, e);
      if (log_rec == NULL)
        {
          return e->cause_code;
        }
    }

  // Switch back to write mode
  return SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////
/////////////////////// UNDO (Figure 12)

err_t
pgr_restart_undo (struct pager *p, struct aries_ctx *ctx, error *e)
{
  i_log_info ("Pgr Restart Undo\n");

  // WHILE EXISTS Trans_Table entry with Status=U DO
  while (true)
    {
      // UndoLsn = maximum(UndoNxtLSN) from Trans_Table entries with State = 'U'
      slsn undo_lsn = txnt_max_u_undo_lsn (&ctx->txt);

      // !EXISTS Trans_Table entry with Status=U
      if (undo_lsn < 0)
        {
          break;
        }

      // LogRec = LogRead(UndoNxtLSN)
      struct wal_rec_hdr_read *log_rec = wal_read_entry (&p->ww, undo_lsn, e);
      if (log_rec == NULL)
        {
          return e->cause_code;
        }

      switch (log_rec->type)
        {
        case WL_UPDATE:
          {
            // Save these because they get overridden on write
            struct txn *tx;
            txid tid = log_rec->update.tid;
            pgno prev = log_rec->update.prev;

            // IF LogRec is undoable THEN DO;
            {
              // fix&latch(LogRec.PageID, 'X')
              page_h ph = page_h_create ();
              err_t_wrap (pgr_get_unverified (&ph, log_rec->update.pg, p, e), e);
              err_t_wrap (pgr_make_writable_no_tx (p, &ph, e), e);

              // Undo_Update(Page, LogRec)
              i_memcpy (page_h_w (&ph)->raw, log_rec->update.undo, PAGE_SIZE);

              txnt_get_expect (&tx, &ctx->txt, tid);

              slsn l = wal_append_clr_log (
                  &p->ww,
                  (struct wal_clr_write){
                      .tid = log_rec->update.tid,
                      .prev = tx->data.last_lsn,
                      .pg = log_rec->update.pg,
                      .undo_next = log_rec->update.prev,
                      .redo = log_rec->update.undo,
                  },
                  e);
              err_t_wrap (l, e);
              err_t_wrap (wal_flush_to (&p->ww, l, e), e);

              // Page.LSN = LgLSN
              page_set_page_lsn (page_h_w (&ph), l);
              txn_update_last (tx, l);

              // unfix&unlatch(page)
              pgr_release_no_tx (p, &ph, PG_ANY, NULL);
            } // END;

            // Trans_Table[LogRec.TransID].UndoNxtLSN :+ LogRec.PrevLSN
            txn_update_undo_next (tx, prev);
            break;
          }
        case WL_CLR:
          {
            struct txn *tx;
            txnt_get_expect (&tx, &ctx->txt, log_rec->clr.tid);
            txn_update_undo_next (tx, log_rec->clr.undo_next);
            break;
          }

          // If LogRec.PrevLSN == 0 THEN
        case WL_BEGIN:
          {
            // Log_Write('end', LogRec.TransID, Trans_Table[LogRec[LogRec.TransId].LastLSN, ...);
            err_t_wrap (wal_append_end_log (&p->ww, log_rec->begin.tid, undo_lsn, e), e);

            // delete Trans_Table entry where TransID = LogRec.TransID
            struct txn *tx;
            txnt_get_expect (&tx, &ctx->txt, log_rec->begin.tid);
            err_t_wrap (txnt_remove_txn_expect (&ctx->txt, tx, e), e);
            break;
          }
        default:
          {
            break;
          }
        }
    }

  return SUCCESS;
}
