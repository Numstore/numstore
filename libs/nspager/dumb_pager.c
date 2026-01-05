#include <config.h>
#include <file_pager.h>
#include <numstore/core/error.h>
#include <numstore/intf/os.h>
#include <numstore/intf/types.h>
#include <numstore/pager.h>
#include <numstore/pager/page.h>
#include <numstore/pager/page_h.h>
#include <numstore/pager/root_node.h>
#include <numstore/pager/tombstone.h>

#ifdef DUMB_PAGER

struct pager
{
  struct file_pager fp;
};

// Helper to allocate a page_frame
static struct page_frame *
alloc_page_frame (error *e)
{
  struct page_frame *pf = i_calloc (1, sizeof (struct page_frame), e);
  if (pf)
    {
      pf->pin = 0;
      pf->flags = 0;
      pf->wsibling = -1;
      latch_init (&pf->latch);
    }
  return pf;
}

///////////////////////////////////////////////////
//////// Lifecycle

struct pager *
pgr_open (
    const char *fname,
    const char *walname,
    struct lockt *t,
    struct thread_pool *tp,
    error *e)
{
  (void)walname; // Unused - no WAL in dumb pager

  struct pager *p = i_calloc (1, sizeof *p, e);
  if (!p)
    return NULL;

  bool is_new = !i_exists_rw (fname);

  if (fpgr_open (&p->fp, fname, e))
    {
      i_free (p);
      return NULL;
    }

  if (is_new)
    {
      // Reset creates empty file
      if (fpgr_reset (&p->fp, e))
        {
          fpgr_close (&p->fp, NULL);
          i_free (p);
          return NULL;
        }

      // Create root page
      page root;
      page_init_empty (&root, PG_ROOT_NODE);
      root.pg = 0;
      rn_set_first_tmbst (&root, 1);
      rn_set_master_lsn (&root, 0);

      pgno pg;
      if (fpgr_new (&p->fp, &pg, e) || fpgr_write (&p->fp, root.raw, pg, e))
        {
          fpgr_close (&p->fp, NULL);
          i_free (p);
          return NULL;
        }
    }

  return p;
}

err_t
pgr_close (struct pager *p, error *e)
{
  if (!p)
    return SUCCESS;

  err_t ret = fpgr_close (&p->fp, e);
  i_free (p);
  return ret;
}

///////////////////////////////////////////////////
//////// Utils

p_size
pgr_get_npages (const struct pager *p)
{
  return fpgr_get_npages (&p->fp);
}

void
i_log_page_table (int log_level, struct pager *p)
{
  (void)log_level;
  (void)p;
  // No page table in dumb pager
}

///////////////////////////////////////////////////
//////// Transaction control

err_t
pgr_begin_txn (struct txn *tx, struct pager *p, error *e)
{
  (void)tx;
  (void)p;
  (void)e;
  return SUCCESS; // No-op
}

err_t
pgr_commit (struct pager *p, struct txn *tx, error *e)
{
  (void)p;
  (void)tx;
  (void)e;
  return SUCCESS; // No-op
}

err_t
pgr_checkpoint (struct pager *p, error *e)
{
  (void)p;
  (void)e;
  return SUCCESS; // No-op
}

///////////////////////////////////////////////////
//////// Page fetching

err_t
pgr_get (page_h *dest, int flags, pgno pg, struct pager *p, error *e)
{
  ASSERT (dest->mode == PHM_NONE);

  if (pg >= fpgr_get_npages (&p->fp))
    {
      return error_causef (e, ERR_PG_OUT_OF_RANGE, "Page is out of range");
    }

  // Allocate page frame
  struct page_frame *pgr = alloc_page_frame (e);
  if (!pgr)
    {
      return e->cause_code;
    }

  // Read from file
  err_t ret = fpgr_read (&p->fp, pgr->page.raw, pg, e);
  if (ret)
    {
      i_free (pgr);
      return ret;
    }
  pgr->page.pg = pg;
  pgr->pin = 1;

  // Validate if requested
  if (flags != PG_ANY)
    {
      ret = page_validate_for_db (&pgr->page, flags, e);
      if (ret)
        {
          i_free (pgr);
          return ret;
        }
    }

  dest->pgr = pgr;
  dest->pgw = NULL;
  dest->mode = PHM_S;
  dest->tx = NULL;

  return SUCCESS;
}

err_t
pgr_new (page_h *dest, struct pager *p, struct txn *tx, enum page_type ptype, error *e)
{
  ASSERT (dest->mode == PHM_NONE);

  // Read root to get first tombstone
  page_h root = page_h_create ();
  err_t ret = pgr_get (&root, PG_ROOT_NODE, 0, p, e);
  if (ret)
    return ret;

  pgno new_pg = rn_get_first_tmbst (page_h_ro (&root));
  pgno next_tombstone;

  if (new_pg < fpgr_get_npages (&p->fp))
    {
      // Reuse tombstone page - read it first to get next pointer
      page_h tombstone = page_h_create ();
      ret = pgr_get (&tombstone, PG_TOMBSTONE, new_pg, p, e);
      if (ret)
        {
          pgr_release (p, &root, PG_ROOT_NODE, NULL);
          return ret;
        }

      next_tombstone = tmbst_get_next (page_h_ro (&tombstone));

      // Convert to writable for reuse
      ret = pgr_make_writable (p, tx, &tombstone, e);
      if (ret)
        {
          pgr_release (p, &tombstone, PG_TOMBSTONE, NULL);
          pgr_release (p, &root, PG_ROOT_NODE, NULL);
          return ret;
        }

      // Initialize as new page type
      page_init_empty (page_h_w (&tombstone), ptype);

      // Transfer ownership to dest
      *dest = tombstone;
    }
  else
    {
      // Extend file
      ret = fpgr_new (&p->fp, &new_pg, e);
      if (ret)
        {
          pgr_release (p, &root, PG_ROOT_NODE, NULL);
          return ret;
        }

      next_tombstone = fpgr_get_npages (&p->fp);

      // Allocate new page frames
      struct page_frame *pgr = alloc_page_frame (e);
      struct page_frame *pgw = alloc_page_frame (e);
      if (!pgr || !pgw)
        {
          if (pgr)
            i_free (pgr);
          if (pgw)
            i_free (pgw);
          pgr_release (p, &root, PG_ROOT_NODE, NULL);
          return e->cause_code;
        }

      page_init_empty (&pgw->page, ptype);
      pgw->page.pg = new_pg;
      pgr->page.pg = new_pg;
      pgr->pin = 1;
      pgw->pin = 1;
      pgr->wsibling = 0; // Dummy value, pgw is not in an array

      dest->pgr = pgr;
      dest->pgw = pgw;
      dest->mode = PHM_X;
      dest->tx = tx;
    }

  // Update root's tombstone pointer
  ret = pgr_make_writable (p, tx, &root, e);
  if (ret)
    {
      pgr_release (p, dest, ptype, NULL);
      pgr_release (p, &root, PG_ROOT_NODE, NULL);
      return ret;
    }

  rn_set_first_tmbst (page_h_w (&root), next_tombstone);
  ret = pgr_save (p, &root, PG_ROOT_NODE, e);
  if (ret)
    {
      pgr_release (p, dest, ptype, NULL);
      pgr_release (p, &root, PG_ROOT_NODE, NULL);
      return ret;
    }

  pgr_release (p, &root, PG_ROOT_NODE, NULL);

  return SUCCESS;
}

err_t
pgr_get_unverified (page_h *dest, pgno pg, struct pager *p, error *e)
{
  return pgr_get (dest, PG_ANY, pg, p, e);
}

err_t
pgr_new_blank (page_h *dest, struct pager *p, struct txn *tx, enum page_type ptype, error *e)
{
  return pgr_new (dest, p, tx, ptype, e);
}

err_t
pgr_make_writable (struct pager *p, struct txn *tx, page_h *h, error *e)
{
  (void)p;
  ASSERT (h->mode == PHM_S);

  // Allocate write buffer
  struct page_frame *pgw = alloc_page_frame (e);
  if (!pgw)
    return e->cause_code;

  // Copy read page to write page
  i_memcpy (pgw->page.raw, h->pgr->page.raw, PAGE_SIZE);
  pgw->page.pg = h->pgr->page.pg;
  pgw->pin = 1;

  h->pgw = pgw;
  h->pgr->wsibling = 0; // Dummy value
  h->mode = PHM_X;
  h->tx = tx;

  return SUCCESS;
}

err_t
pgr_maybe_make_writable (struct pager *p, struct txn *tx, page_h *cur, error *e)
{
  if (cur->mode == PHM_S)
    {
      return pgr_make_writable (p, tx, cur, e);
    }
  return SUCCESS;
}

///////////////////////////////////////////////////
//////// Shorthands

err_t
pgr_get_writable (page_h *dest, struct txn *tx, int flags, pgno pg, struct pager *p, error *e)
{
  err_t ret = pgr_get (dest, flags, pg, p, e);
  if (ret)
    return ret;

  ret = pgr_make_writable (p, tx, dest, e);
  if (ret)
    {
      // Free the read page on failure
      i_free (dest->pgr);
      dest->mode = PHM_NONE;
      dest->pgr = NULL;
    }
  return ret;
}

err_t
pgr_get_writable_no_tx (page_h *dest, int flags, pgno pg, struct pager *p, error *e)
{
  return pgr_get_writable (dest, NULL, flags, pg, p, e);
}

///////////////////////////////////////////////////
//////// Save and log changes to WAL if any

err_t
pgr_save (struct pager *p, page_h *h, int flags, error *e)
{
  ASSERT (h->mode == PHM_X);

  // Validate before writing
  err_t_wrap (page_validate_for_db (page_h_w (h), flags, e), e);

  // Write directly to file
  pgno pg = page_h_pgno (h);
  err_t_wrap (fpgr_write (&p->fp, page_h_w (h)->raw, pg, e), e);

  // Copy to read buffer and downgrade to read mode
  i_memcpy (h->pgr->page.raw, h->pgw->page.raw, PAGE_SIZE);

  // Free write buffer
  i_free (h->pgw);
  h->pgw = NULL;
  h->pgr->wsibling = -1;
  h->mode = PHM_S;

  return SUCCESS;
}

err_t
pgr_delete_and_release (struct pager *p, struct txn *tx, page_h *h, error *e)
{
  // Get root page writable
  page_h root = page_h_create ();
  err_t ret = pgr_get_writable (&root, tx, PG_ROOT_NODE, 0, p, e);
  if (ret)
    return ret;

  // Make h writable if needed
  if (h->mode == PHM_S)
    {
      ret = pgr_make_writable (p, tx, h, e);
      if (ret)
        {
          pgr_release (p, &root, PG_ROOT_NODE, NULL);
          return ret;
        }
    }

  // Convert page to tombstone
  pgno old_first = rn_get_first_tmbst (page_h_ro (&root));
  page_init_empty (page_h_w (h), PG_TOMBSTONE);
  tmbst_set_next (page_h_w (h), old_first);

  // Save tombstone page
  ret = pgr_save (p, h, PG_TOMBSTONE, e);
  if (ret)
    {
      pgr_release (p, &root, PG_ROOT_NODE, NULL);
      pgr_release (p, h, PG_TOMBSTONE, NULL);
      return ret;
    }

  // Update root
  rn_set_first_tmbst (page_h_w (&root), page_h_pgno (h));
  ret = pgr_save (p, &root, PG_ROOT_NODE, e);
  if (ret)
    {
      pgr_release (p, &root, PG_ROOT_NODE, NULL);
      pgr_release (p, h, PG_TOMBSTONE, NULL);
      return ret;
    }

  // Release both
  pgr_release (p, &root, PG_ROOT_NODE, NULL);
  pgr_release (p, h, PG_TOMBSTONE, NULL);

  return SUCCESS;
}

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
  if (h->mode == PHM_X)
    {
      err_t ret = pgr_save (p, h, flags, e);
      if (ret)
        {
          // Free both buffers on error
          if (h->pgr)
            i_free (h->pgr);
          if (h->pgw)
            i_free (h->pgw);
          h->mode = PHM_NONE;
          h->pgr = NULL;
          h->pgw = NULL;
          h->tx = NULL;
          return ret;
        }
    }

  // Free read buffer (write buffer already freed in save)
  if (h->pgr)
    i_free (h->pgr);
  if (h->pgw)
    i_free (h->pgw); // Should be NULL if save succeeded

  h->mode = PHM_NONE;
  h->pgr = NULL;
  h->pgw = NULL;
  h->tx = NULL;

  return SUCCESS;
}

err_t
pgr_flush_wall (struct pager *p, error *e)
{
  (void)p;
  (void)e;
  return SUCCESS; // No WAL to flush
}

///////////////////////////////////////////////////
//////// ARIES

err_t
pgr_rollback (struct pager *p, struct txn *tx, lsn save_lsn, error *e)
{
  (void)p;
  (void)tx;
  (void)save_lsn;
  (void)e;
  return SUCCESS; // No rollback in dumb pager
}

#ifndef NTEST
err_t
pgr_crash (struct pager *p, error *e)
{
  if (!p)
    return SUCCESS;

  // Just close without flushing
  fpgr_close (&p->fp, NULL);
  i_free (p);

  return SUCCESS;
}
#endif
#endif
