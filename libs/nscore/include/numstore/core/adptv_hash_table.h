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
 *   Adaptive hash table implementation that dynamically resizes based on load
 *   factor. Supports concurrent incremental rehashing and configurable size bounds.
 */

#include <numstore/core/hash_table.h>
#include <numstore/core/latch.h>

struct adptv_htable_settings
{
  u32 max_load_factor;
  u32 min_load_factor;
  u32 rehashing_work;
  u32 max_size;
  u32 min_size;
};

struct adptv_htable
{
  struct htable *current;
  struct htable *prev;
  u32 migrate_pos;
  struct latch latch;

  struct adptv_htable_settings settings;
};

err_t adptv_htable_init (struct adptv_htable *dest, struct adptv_htable_settings settings, error *e);
void adptv_htable_free (struct adptv_htable *dest);

struct hnode *adptv_htable_lookup (
    struct adptv_htable *t,
    const struct hnode *key,
    bool (*eq) (const struct hnode *, const struct hnode *));

err_t adptv_htable_insert (struct adptv_htable *t, struct hnode *node, error *e);

err_t adptv_htable_delete (
    struct hnode **dest,
    struct adptv_htable *t,
    const struct hnode *key,
    bool (*eq) (const struct hnode *, const struct hnode *),
    error *e);

// Simple getters
HEADER_FUNC u32
adptv_htable_size (struct adptv_htable *t)
{
  return t->current->size + t->prev->size;
}

// Iterator
void adptv_htable_foreach (const struct adptv_htable *t, void (*action) (struct hnode *v, void *ctx), void *ctx);
