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
 *   Defines structures and functions for tracking and applying updates to inner
 *   nodes during R+ tree modifications, managing left/right node changes and pivot updates.
 */

// numstore
#include <numstore/core/assert.h>
#include <numstore/pager/page_h.h>

#include "config.h"

#define NUPD_LENGTH (MAX_NUPD_SIZE + 5 * IN_MAX_KEYS)
#define NUPD_MAX_DATA_LENGTH (NUPD_LENGTH * DL_DATA_SIZE)

struct node_updates
{
  struct in_pair pivot;
  struct in_pair right[NUPD_LENGTH];
  struct in_pair left[NUPD_LENGTH];
  struct in_pair *prev;

  // Length
  u32 rlen;
  u32 llen;

  // Observed
  u32 robs;
  u32 lobs;

  // Consumed
  u32 rcons;
  u32 lcons;
};

#define pgh_unravel(pg) page_h_pgno (pg), dlgt_get_size (page_h_ro (pg))

struct in_pair *nupd_init (struct node_updates *s, pgno pg, b_size size);

void nupd_commit_1st_right (struct node_updates *s, pgno pg, b_size size);
void nupd_commit_1st_left (struct node_updates *s, pgno pg, b_size size);

void nupd_append_2nd_right (struct node_updates *s, pgno pg1, b_size size1, pgno pg2, b_size size2);
void nupd_append_2nd_left (struct node_updates *s, pgno pg1, b_size size1, pgno pg2, b_size size2);

void nupd_append_tip_right (struct node_updates *s, struct three_in_pair output);
void nupd_append_tip_left (struct node_updates *s, struct three_in_pair output);

void nupd_observe_pivot (struct node_updates *s, page_h *pg, p_size lidx);
void nupd_observe_right_from (struct node_updates *s, page_h *pg, p_size lidx);
void nupd_observe_left_from (struct node_updates *s, page_h *pg, p_size lidx);
void nupd_observe_all_right (struct node_updates *s, page_h *pg);
void nupd_observe_all_left (struct node_updates *s, page_h *pg);

struct in_pair nupd_consume_right (struct node_updates *s);
struct in_pair nupd_consume_left (struct node_updates *s);

bool nupd_done_observing_left (struct node_updates *s);
bool nupd_done_observing_right (struct node_updates *s);

bool nupd_done_consuming_left (struct node_updates *s);
bool nupd_done_consuming_right (struct node_updates *s);

bool nupd_done_left (struct node_updates *s);
bool nupd_done_right (struct node_updates *s);

// UTILS / SHORTHAND
p_size nupd_append_maximally_left (struct node_updates *n, page_h *pg, p_size rlen);
p_size nupd_append_maximally_right (struct node_updates *n, page_h *pg, p_size llen);
p_size nupd_append_maximally_left_then_right (struct node_updates *n, page_h *pg);
p_size nupd_append_maximally_right_then_left (struct node_updates *n, page_h *pg);
