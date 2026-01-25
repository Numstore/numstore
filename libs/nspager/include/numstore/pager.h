#pragma once

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
 *   Main pager interface providing page management, transaction support,
 *   and database file operations with write-ahead logging for crash recovery.
 */

#include <numstore/core/threadpool.h>
#include <numstore/pager/lock_table.h>
#include <numstore/pager/page_h.h>
#include <numstore/pager/txn.h>

// Special Page Numbers
#define ROOT_PGNO ((pgno)0)  // Root page
#define VHASH_PGNO ((pgno)1) // Variable hash table page

// Lifecycle
struct pager *pgr_open (const char *fname, const char *walname, struct lockt *lt, struct thread_pool *tp, error *e);
err_t pgr_close (struct pager *p, error *e);

// Utils
p_size pgr_get_npages (const struct pager *p);
void i_log_page_table (int log_level, struct pager *p);

// Transaction control
err_t pgr_begin_txn (struct txn *tx, struct pager *p, error *e);
err_t pgr_commit (struct pager *p, struct txn *tx, error *e);
err_t pgr_checkpoint (struct pager *p, error *e); // Blocking - should be called in a sepearte thread

// Page fetching
err_t pgr_get (page_h *dest, int flags, pgno pgno, struct pager *p, error *e);
err_t pgr_get_unverified (page_h *dest, pgno pgno, struct pager *p, error *e);
err_t pgr_new (page_h *dest, struct pager *p, struct txn *tx, enum page_type ptype, error *e);

// Make writable
err_t pgr_make_writable (struct pager *p, struct txn *tx, page_h *h, error *e);
err_t pgr_maybe_make_writable (struct pager *p, struct txn *tx, page_h *cur, error *e);
err_t pgr_make_writable_no_tx (struct pager *p, page_h *h, error *e);

// Shorthand (get + make writable)
err_t pgr_get_writable (page_h *dest, struct txn *tx, int flags, pgno pg, struct pager *p, error *e);
err_t pgr_get_writable_no_tx (page_h *dest, int flags, pgno pg, struct pager *p, error *e);

// Save and log changes to WAL if any
err_t pgr_save (struct pager *p, page_h *h, int flags, error *e);
err_t pgr_delete_and_release (struct pager *p, struct txn *tx, page_h *h, error *e);

// Release
err_t pgr_release_if_exists (struct pager *p, page_h *h, int flags, error *e);
err_t pgr_release (struct pager *p, page_h *h, int flags, error *e);
err_t pgr_release_no_tx (struct pager *p, page_h *h, int flags, error *e);

err_t pgr_flush_wall (struct pager *p, error *e);

// ARIES
err_t pgr_rollback (struct pager *p, struct txn *tx, lsn save_lsn, error *e);

#ifndef NTEST
err_t pgr_crash (struct pager *p, error *e);
#endif
