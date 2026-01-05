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
 *   High-level pager utility routines for common page operations like allocation, navigation, and transaction management.
 */

// numstore
#include <numstore/pager.h>
#include <numstore/pager/inner_node.h>

// Deleting nodes
err_t pgr_dlgt_delete_and_fill_next (page_h *cur, page_h *next, struct txn *tx, struct pager *p, error *e);
err_t pgr_dlgt_delete_and_fill_prev (page_h *prev, page_h *cur, struct txn *tx, struct pager *p, error *e);
err_t pgr_dlgt_delete_chain (page_h *cur, struct txn *tx, struct pager *p, error *e);
err_t pgr_dlgt_delete_vpvl_chain (page_h *cur, struct txn *tx, struct pager *P, error *e);

// Fetching / Creating neighbors
err_t pgr_dlgt_get_next (page_h *cur, page_h *next, struct txn *tx, struct pager *p, error *e);
err_t pgr_dlgt_get_prev (page_h *cur, page_h *prev, struct txn *tx, struct pager *p, error *e);
err_t pgr_dlgt_get_ovnext (page_h *cur, page_h *next, struct txn *tx, struct pager *p, error *e);
err_t pgr_dlgt_get_neighbors (page_h *prev, page_h *cur, page_h *next, struct txn *tx, struct pager *p, error *e);
err_t pgr_dlgt_new_next_no_link (page_h *cur, page_h *next, struct txn *tx, struct pager *p, error *e);
err_t pgr_dlgt_new_prev_no_link (page_h *cur, page_h *prev, struct txn *tx, struct pager *p, error *e);
err_t pgr_dlgt_new_ovnext_no_link (page_h *cur, page_h *prev, struct txn *tx, struct pager *p, error *e);
err_t pgr_dlgt_new_next (page_h *cur, page_h *dest, page_h *c_next, struct txn *tx, struct pager *p, error *e);
err_t pgr_dlgt_new_prev (page_h *c_prev, page_h *dest, page_h *cur, struct txn *tx, struct pager *p, error *e);

// Advancing
err_t pgr_dlgt_advance_next (page_h *cur, struct txn *tx, struct pager *p, error *e);
err_t pgr_dlgt_advance_prev (page_h *cur, struct txn *tx, struct pager *p, error *e);
err_t pgr_dlgt_advance_new_next (page_h *cur, page_h *c_next, struct txn *tx, struct pager *p, error *e);
err_t pgr_dlgt_advance_new_prev (page_h *c_prev, page_h *cur, struct txn *tx, struct pager *p, error *e);
err_t pgr_dlgt_advance_new_next_no_link (page_h *cur, struct txn *tx, struct pager *p, error *e);
err_t pgr_dlgt_advance_new_prev_no_link (page_h *cur, struct txn *tx, struct pager *p, error *e);

// Balancing nodes
void dlgt_balance_with_next (page_h *cur, page_h *next);
void dlgt_balance_with_prev (page_h *prev, page_h *cur);
