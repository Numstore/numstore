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
 *   Implements adptv_hash_table.h. Adaptive hash table with dynamic resizing.
 */

#include <numstore/core/adptv_hash_table.h>

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/core/hash_table.h>
#include <numstore/core/latch.h>

DEFINE_DBG_ASSERT (
    struct adptv_htable_settings, adptv_htable_settings, t,
    {
      ASSERT (t);
      ASSERT (t->min_size > 0);
      ASSERT (t->max_size > 0);
      ASSERT (t->min_size <= t->max_size);
      ASSERT (t->rehashing_work > 0);
      ASSERT (t->min_load_factor > 0);
      ASSERT (t->max_load_factor > 0);
    })

DEFINE_DBG_ASSERT (struct adptv_htable, adptv_htable, t, {
  ASSERT (t);
  ASSERT (t->current);
  ASSERT (t->prev);
  ASSERT (t->migrate_pos <= t->prev->cap);
  DBG_ASSERT (adptv_htable_settings, &t->settings);
})

err_t
adptv_htable_init (struct adptv_htable *dest, struct adptv_htable_settings settings, error *e)
{
  dest->current = htable_create (settings.min_size, e);
  if (dest->current == NULL)
    {
      return e->cause_code;
    }

  dest->prev = htable_create (settings.min_size, e);
  if (dest->prev == NULL)
    {
      htable_free (dest->current);
      return e->cause_code;
    }

  dest->settings = settings;
  dest->migrate_pos = 0;
  latch_init (&dest->latch);

  DBG_ASSERT (adptv_htable, dest);

  return SUCCESS;
}

void
adptv_htable_free (struct adptv_htable *t)
{
  DBG_ASSERT (adptv_htable, t);

  htable_free (t->current);
  htable_free (t->prev);

  t->current = NULL;
  t->prev = NULL;
}

static void
adptv_htable_finish_rehashing (struct adptv_htable *t)
{
  DBG_ASSERT (adptv_htable, t);
  while (t->prev->size > 0)
    {
      DBG_ASSERT (adptv_htable, t);
      struct hnode **from = &t->prev->table[t->migrate_pos];
      if (!*from)
        {
          t->migrate_pos++;
          continue;
        }

      htable_insert (t->current, htable_delete (t->prev, from));
    }
}

static void
adptv_htable_help_rehashing (struct adptv_htable *t)
{
  DBG_ASSERT (adptv_htable, t);
  u32 work = 0;
  while (work < t->settings.rehashing_work && t->prev->size > 0)
    {
      DBG_ASSERT (adptv_htable, t);
      work++;

      struct hnode **from = &t->prev->table[t->migrate_pos];
      if (!*from)
        {
          t->migrate_pos++;
          continue;
        }

      htable_insert (t->current, htable_delete (t->prev, from));
    }
}

struct hnode *
adptv_htable_lookup (
    struct adptv_htable *t,
    const struct hnode *key,
    bool (*eq) (const struct hnode *, const struct hnode *))
{
  latch_lock (&t->latch);

  DBG_ASSERT (adptv_htable, t);

  // Look up from current
  struct hnode **from = htable_lookup (t->current, key, eq);
  if (!from)
    {
      // Failed - maybe look up from prev
      from = htable_lookup (t->prev, key, eq);
    }

  // Doesn't exist
  if (!from)
    {
      latch_unlock (&t->latch);
      return NULL;
    }

  struct hnode *result = *from;

  latch_unlock (&t->latch);

  return result;
}

static inline err_t
adptv_htable_trigger_rehashing (struct adptv_htable *t, u32 newcap, error *e)
{
  DBG_ASSERT (adptv_htable, t);
  adptv_htable_finish_rehashing (t);

  struct htable *prev = t->prev;
  struct htable *current = t->current;
  struct htable *new = htable_create (newcap, e);
  if (new == NULL)
    {
      return e->cause_code;
    }

  htable_free (prev);
  t->prev = current;
  t->current = new;
  t->migrate_pos = 0;

  return SUCCESS;
}

err_t
adptv_htable_insert (struct adptv_htable *t, struct hnode *node, error *e)
{
  latch_lock (&t->latch);

  DBG_ASSERT (adptv_htable, t);

  u32 size = t->current->size + t->prev->size;
  u32 threshold = t->current->cap * t->settings.max_load_factor;
  if (size + 1 >= threshold)
    {
      u32 newcap = t->current->cap * 2;
      if (newcap <= t->settings.max_size && adptv_htable_trigger_rehashing (t, t->current->cap * 2, e))
        {
          latch_unlock (&t->latch);
          return e->cause_code;
        }
    }

  htable_insert (t->current, node);

  adptv_htable_help_rehashing (t);

  latch_unlock (&t->latch);

  return SUCCESS;
}

err_t
adptv_htable_delete (
    struct hnode **dest,
    struct adptv_htable *t,
    const struct hnode *key, bool (*eq) (const struct hnode *, const struct hnode *),
    error *e)
{
  latch_lock (&t->latch);

  DBG_ASSERT (adptv_htable, t);
  struct hnode **from;

  u32 size = t->current->size + t->prev->size;
  u32 threshold = (t->current->cap + 1) * t->settings.min_load_factor;
  if (size <= threshold + 1)
    {
      u32 newcap = t->current->cap / 2;
      if (newcap >= t->settings.min_size && adptv_htable_trigger_rehashing (t, newcap, e))
        {
          latch_unlock (&t->latch);
          return e->cause_code;
        }
    }

  from = htable_lookup (t->current, key, eq);
  if (from)
    {
      struct hnode *_dest = htable_delete (t->current, from);
      if (dest)
        {
          *dest = _dest;
        }
      latch_unlock (&t->latch);
      return SUCCESS;
    }

  from = htable_lookup (t->prev, key, eq);
  if (from)
    {
      struct hnode *_dest = htable_delete (t->prev, from);
      if (dest)
        {
          *dest = _dest;
        }
      latch_unlock (&t->latch);
      return SUCCESS;
    }

  *dest = NULL;

  latch_unlock (&t->latch);

  return SUCCESS;
}

void
adptv_htable_foreach (const struct adptv_htable *t, void (*action) (struct hnode *v, void *ctx), void *ctx)
{
  latch_lock (&((struct adptv_htable *)t)->latch);

  htable_foreach (t->prev, action, ctx);
  htable_foreach (t->current, action, ctx);

  latch_unlock (&((struct adptv_htable *)t)->latch);
}
