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
 *   Builder pattern implementation for constructing struct and union types by
 *   incrementally accepting key-value-type triples and building final type structures.
 */

#include <numstore/core/llist.h>
#include <numstore/core/string.h>
#include <numstore/types/types.h>

struct kv_llnode
{
  struct string key;
  struct type value;
  struct llnode link;
};

struct kvt_builder
{
  struct llnode *head;

  u16 klen;
  u16 tlen;

  struct chunk_alloc *temp;       // worker data
  struct chunk_alloc *persistent; // persistent memory data
};

void kvb_create (struct kvt_builder *dest, struct chunk_alloc *temp, struct chunk_alloc *persistent);

// Accept stuff
err_t kvb_accept_key (struct kvt_builder *ub, struct string key, error *e);
err_t kvb_accept_type (struct kvt_builder *eb, struct type t, error *e);

// Build
err_t kvb_union_t_build (struct union_t *dest, struct kvt_builder *eb, error *e);
err_t kvb_struct_t_build (struct struct_t *dest, struct kvt_builder *eb, error *e);
