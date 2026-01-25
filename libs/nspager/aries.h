#pragma once

#include <numstore/intf/types.h>
#include <numstore/pager.h>
#include <numstore/pager/dirty_page_table.h>
#include <numstore/pager/txn_table.h>

struct aries_ctx
{
  // Input
  lsn master_lsn;

  // Intermediate
  lsn redo_lsn;

  // Hash table of transactions
  struct txn_table txt;

  // List of open transactions
  struct dbl_buffer txns;

  // Dirty page table
  struct dpg_table dpt;
  txid max_tid;
};

err_t aries_ctx_create (struct aries_ctx *dest, lsn master_lsn, error *e);
void aries_ctx_free (struct aries_ctx *ctx);

err_t pgr_restart (struct pager *p, struct aries_ctx *ctx, error *e);
err_t pgr_rollback (struct pager *p, struct txn *tx, lsn save_lsn, error *e);
err_t pgr_restart_analysis (struct pager *p, struct aries_ctx *ctx, error *e);
err_t pgr_restart_redo (struct pager *p, struct aries_ctx *ctx, error *e);
err_t pgr_restart_undo (struct pager *p, struct aries_ctx *ctx, error *e);
