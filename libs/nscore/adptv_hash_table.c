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
  err_t ret = latch_init (&dest->latch, e);
  if (ret < SUCCESS)
    {
      htable_free (dest->current);
      htable_free (dest->prev);
      return ret;
    }

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

      struct hnode **from = &t->prev->table[t->migrate_pos];
      if (*from == NULL)
        {
          t->migrate_pos++;
          continue;
        }

      work++;
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

  bool needs_rehash = adptv_htable_size (t) + 1 >= (t->current->cap * t->settings.max_load_factor);

  if (needs_rehash)
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

  bool needs_rehash = adptv_htable_size (t) <= (t->current->cap * t->settings.min_load_factor) + 1;

  if (needs_rehash)
    {
      u32 newcap = t->current->cap / 2;
      if (newcap >= t->settings.min_size && adptv_htable_trigger_rehashing (t, newcap, e))
        {
          latch_unlock (&t->latch);
          return e->cause_code;
        }
    }

  if (dest)
    {
      *dest = NULL;
    }

  from = htable_lookup (t->current, key, eq);
  if (from)
    {
      struct hnode *_dest = htable_delete (t->current, from);
      if (dest)
        {
          *dest = _dest;
        }
      goto theend;
    }

  from = htable_lookup (t->prev, key, eq);
  if (from)
    {
      struct hnode *_dest = htable_delete (t->prev, from);
      if (dest)
        {
          *dest = _dest;
        }
      goto theend;
    }

theend:
  adptv_htable_help_rehashing (t);

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

#ifndef NTEST
#include <numstore/test/testing.h>

TEST (TT_UNIT, adptv_htable_rehash_insert)
{
  error e;
  struct adptv_htable table;

  struct adptv_htable_settings settings = {
    .min_size = 10,
    .max_size = 50000,
    .rehashing_work = 2,
    .min_load_factor = 0.25,
    .max_load_factor = 1.5,
  };

  adptv_htable_init (&table, settings, &e);

#define INSERT_NEXT(i, psize, csize, pcap, ccap)                         \
  do                                                                     \
    {                                                                    \
      nodes[i].hcode = i;                                                \
      test_err_t_wrap (adptv_htable_insert (&table, &nodes[i], &e), &e); \
      test_assert_int_equal (table.prev->size, psize);                   \
      test_assert_int_equal (table.current->size, csize);                \
      test_assert_int_equal (table.prev->cap, pcap);                     \
      test_assert_int_equal (table.current->cap, ccap);                  \
    }                                                                    \
  while (0)

#define DELETE_NEXT(i, psize, csize, pcap, ccap)                                 \
  do                                                                             \
    {                                                                            \
      struct hnode key = { .hcode = i };                                         \
      struct hnode *dest;                                                        \
      test_err_t_wrap (adptv_htable_delete (&dest, &table, &key, NULL, &e), &e); \
      test_fail_if_null (dest);                                                  \
      test_assert_equal (dest, &nodes[i]);                                       \
      test_assert_int_equal (table.prev->size, psize);                           \
      test_assert_int_equal (table.current->size, csize);                        \
      test_assert_int_equal (table.prev->cap, pcap);                             \
      test_assert_int_equal (table.current->cap, ccap);                          \
    }                                                                            \
  while (0)

  for (int k = 0; k < 10; ++k)
    {
      struct hnode nodes[100];

      // First round - previous is 10
      if (k == 0)
        {
          for (int i = 0; i < 14; i++)
            {
              INSERT_NEXT (i, 0, i + 1, 10, 10);
            }
        }

      // Subsequent, prev was just deleted
      else
        {
          for (int i = 0; i < 14; i++)
            {
              INSERT_NEXT (i, 0, i + 1, 20, 10);
            }
        }

      INSERT_NEXT (14, 12, 3, 10, 20);
      INSERT_NEXT (15, 10, 6, 10, 20);
      INSERT_NEXT (16, 8, 9, 10, 20);
      INSERT_NEXT (17, 6, 12, 10, 20);
      INSERT_NEXT (18, 4, 15, 10, 20);
      INSERT_NEXT (19, 2, 18, 10, 20);

      for (u32 i = 20; i < 29; ++i)
        {
          INSERT_NEXT (i, 0, i + 1, 10, 20);
        }

      INSERT_NEXT (29, 27, 3, 20, 40);
      INSERT_NEXT (30, 25, 6, 20, 40);
      INSERT_NEXT (31, 23, 9, 20, 40);
      INSERT_NEXT (32, 21, 12, 20, 40);
      INSERT_NEXT (33, 19, 15, 20, 40);
      INSERT_NEXT (34, 17, 18, 20, 40);
      INSERT_NEXT (35, 15, 21, 20, 40);
      INSERT_NEXT (36, 13, 24, 20, 40);
      INSERT_NEXT (37, 11, 27, 20, 40);
      INSERT_NEXT (38, 9, 30, 20, 40);
      INSERT_NEXT (39, 7, 33, 20, 40);
      INSERT_NEXT (40, 5, 36, 20, 40);
      INSERT_NEXT (41, 3, 39, 20, 40);
      INSERT_NEXT (42, 1, 42, 20, 40);
      INSERT_NEXT (43, 0, 44, 20, 40);
      INSERT_NEXT (44, 0, 45, 20, 40);

      DELETE_NEXT (44, 0, 44, 20, 40);
      DELETE_NEXT (43, 0, 43, 20, 40);
      DELETE_NEXT (42, 0, 42, 20, 40);
      DELETE_NEXT (41, 0, 41, 20, 40);
      DELETE_NEXT (40, 0, 40, 20, 40);
      DELETE_NEXT (39, 0, 39, 20, 40);
      DELETE_NEXT (38, 0, 38, 20, 40);
      DELETE_NEXT (37, 0, 37, 20, 40);
      DELETE_NEXT (36, 0, 36, 20, 40);
      DELETE_NEXT (35, 0, 35, 20, 40);
      DELETE_NEXT (34, 0, 34, 20, 40);
      DELETE_NEXT (33, 0, 33, 20, 40);
      DELETE_NEXT (32, 0, 32, 20, 40);
      DELETE_NEXT (31, 0, 31, 20, 40);
      DELETE_NEXT (30, 0, 30, 20, 40);
      DELETE_NEXT (29, 0, 29, 20, 40);
      DELETE_NEXT (28, 0, 28, 20, 40);
      DELETE_NEXT (27, 0, 27, 20, 40);
      DELETE_NEXT (26, 0, 26, 20, 40);
      DELETE_NEXT (25, 0, 25, 20, 40);
      DELETE_NEXT (24, 0, 24, 20, 40);
      DELETE_NEXT (23, 0, 23, 20, 40);
      DELETE_NEXT (22, 0, 22, 20, 40);
      DELETE_NEXT (21, 0, 21, 20, 40);
      DELETE_NEXT (20, 0, 20, 20, 40);
      DELETE_NEXT (19, 0, 19, 20, 40);
      DELETE_NEXT (18, 0, 18, 20, 40);
      DELETE_NEXT (17, 0, 17, 20, 40);
      DELETE_NEXT (16, 0, 16, 20, 40);
      DELETE_NEXT (15, 0, 15, 20, 40);
      DELETE_NEXT (14, 0, 14, 20, 40);
      DELETE_NEXT (13, 0, 13, 20, 40);
      DELETE_NEXT (12, 0, 12, 20, 40);
      DELETE_NEXT (11, 0, 11, 20, 40);

      DELETE_NEXT (10, 8, 2, 40, 20);
      DELETE_NEXT (9, 5, 4, 40, 20);
      DELETE_NEXT (8, 2, 6, 40, 20);
      DELETE_NEXT (7, 0, 7, 40, 20);
      DELETE_NEXT (6, 0, 6, 40, 20);

      DELETE_NEXT (5, 3, 2, 20, 10);
      DELETE_NEXT (4, 0, 4, 20, 10);
      DELETE_NEXT (3, 0, 3, 20, 10);
      DELETE_NEXT (2, 0, 2, 20, 10);
      DELETE_NEXT (1, 0, 1, 20, 10);
      DELETE_NEXT (0, 0, 0, 20, 10);
    }

  adptv_htable_free (&table);
}
#endif
