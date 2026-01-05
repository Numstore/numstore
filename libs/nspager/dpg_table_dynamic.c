
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
 *   TODO: A dynamic dirty page table. See dirty page table
 */

#include <numstore/pager/dpg_table_dynamic.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <numstore/core/error.h>
#include <numstore/core/macros.h>

static bool
dpg_entry_equals (const struct hnode *left, const struct hnode *right)
{
  const struct dpg_entry_dynamic *l = container_of (left, const struct dpg_entry_dynamic, node);
  const struct dpg_entry_dynamic *r = container_of (right, const struct dpg_entry_dynamic, node);
  return l->pg == r->pg;
}

err_t
dpgt_dyn_open (struct dpg_table_dynamic *dest, error *e)
{
  err_t err;
  struct adptv_htable_settings settings = {
    .max_load_factor = 8,
    .min_load_factor = 1,
    .rehashing_work = 28,
    .max_size = 2048,
    .min_size = 10,
  };

  err = adptv_htable_init (&dest->t, settings, e);
  if (err != SUCCESS)
    {
      return err;
    }

  latch_init (&dest->l);
  return SUCCESS;
}

err_t
dpgt_dyn_close (struct dpg_table_dynamic *t, error *e)
{
  // Free all entries
  struct hnode *node;
  while ((node = adptv_htable_lookup (&t->t, NULL, NULL)) != NULL)
    {
      struct dpg_entry_dynamic *entry = container_of (node, struct dpg_entry_dynamic, node);
      adptv_htable_delete (&node, &t->t, &entry->node, dpg_entry_equals, e);
      i_free (entry);
    }

  adptv_htable_free (&t->t);

  return e->cause_code;
}

err_t
dpgt_dyn_add (struct dpg_table_dynamic *t, pgno pg, lsn rec_lsn, error *e)
{
  err_t err;

  // Check if already exists
  struct dpg_entry_dynamic key = { .pg = pg };
  hnode_init (&key.node, pg);

  latch_lock (&t->l);
  struct hnode *existing = adptv_htable_lookup (&t->t, &key.node, dpg_entry_equals);
  latch_unlock (&t->l);

  if (existing != NULL)
    {
      return error_causef (e, ERR_INVALID_ARGUMENT, "dirty page %lu already exists", pg);
    }

  // MALLOC
  struct dpg_entry_dynamic *entry = malloc (sizeof (struct dpg_entry_dynamic));
  if (entry == NULL)
    {
      return error_causef (e, ERR_NOMEM, "failed to allocate dirty page entry");
    }

  entry->pg = pg;
  entry->rec_lsn = rec_lsn;
  latch_init (&entry->l);
  hnode_init (&entry->node, pg);

  // Insert into table
  latch_lock (&t->l);
  err = adptv_htable_insert (&t->t, &entry->node, e);
  latch_unlock (&t->l);

  if (err != SUCCESS)
    {
      free (entry);
      return err;
    }

  return SUCCESS;
}

err_t
dpgt_dyn_add_or_update (struct dpg_table_dynamic *t, pgno pg, lsn rec_lsn, error *e)
{
  // Try to find existing entry
  struct dpg_entry_dynamic key = { .pg = pg };
  hnode_init (&key.node, pg);

  latch_lock (&t->l);
  struct hnode *node = adptv_htable_lookup (&t->t, &key.node, dpg_entry_equals);

  if (node != NULL)
    {
      /* Update existing */
      struct dpg_entry_dynamic *entry = container_of (node, struct dpg_entry_dynamic, node);
      latch_lock (&entry->l);
      entry->rec_lsn = rec_lsn;
      latch_unlock (&entry->l);
      latch_unlock (&t->l);
      return SUCCESS;
    }
  latch_unlock (&t->l);

  // Doesn't exist, insert new
  return dpgt_dyn_add (t, pg, rec_lsn, e);
}

bool
dpgt_dyn_get (struct dpg_entry_dynamic *dest, struct dpg_table_dynamic *t, pgno pg)
{
  struct dpg_entry_dynamic key = { .pg = pg };
  hnode_init (&key.node, pg);

  latch_lock (&t->l);
  struct hnode *node = adptv_htable_lookup (&t->t, &key.node, dpg_entry_equals);

  if (node == NULL)
    {
      latch_unlock (&t->l);
      return false;
    }

  struct dpg_entry_dynamic *entry = container_of (node, struct dpg_entry_dynamic, node);

  // Copy data
  dest->pg = entry->pg;
  dest->rec_lsn = entry->rec_lsn;

  latch_unlock (&t->l);
  return true;
}

void
dpgt_dyn_get_expect (struct dpg_entry_dynamic *dest, struct dpg_table_dynamic *t, pgno pg)
{
  bool found = dpgt_dyn_get (dest, t, pg);
  assert (found && "expected dirty page to exist");
  (void)found;
}

bool
dpgt_dyn_exists (struct dpg_table_dynamic *t, pgno pg)
{
  struct dpg_entry_dynamic key = { .pg = pg };
  hnode_init (&key.node, pg);

  latch_lock (&t->l);
  struct hnode *node = adptv_htable_lookup (&t->t, &key.node, dpg_entry_equals);
  latch_unlock (&t->l);

  return node != NULL;
}

err_t
dpgt_dyn_remove (bool *exists, struct dpg_table_dynamic *t, pgno pg, error *e)
{
  err_t err;
  struct dpg_entry_dynamic key = { .pg = pg };
  hnode_init (&key.node, pg);

  latch_lock (&t->l);
  struct hnode *node;
  err = adptv_htable_delete (&node, &t->t, &key.node, dpg_entry_equals, e);
  latch_unlock (&t->l);

  if (err != SUCCESS)
    {
      *exists = false;
      return err;
    }

  if (node == NULL)
    {
      *exists = false;
      return SUCCESS;
    }

  *exists = true;

  /* Free the entry */
  struct dpg_entry_dynamic *entry = container_of (node, struct dpg_entry_dynamic, node);
  free (entry);

  return SUCCESS;
}

err_t
dpgt_dyn_remove_expect (struct dpg_table_dynamic *t, pgno pg, error *e)
{
  bool exists;
  err_t err = dpgt_dyn_remove (&exists, t, pg, e);
  assert ((err == SUCCESS && exists) && "expected dirty page to exist");
  return err;
}

void
dpgt_dyn_update (struct dpg_table_dynamic *t, pgno pg, lsn new_rec_lsn)
{
  struct dpg_entry_dynamic key = { .pg = pg };
  hnode_init (&key.node, pg);

  latch_lock (&t->l);
  struct hnode *node = adptv_htable_lookup (&t->t, &key.node, dpg_entry_equals);
  assert (node != NULL && "expected dirty page to exist for update");

  struct dpg_entry_dynamic *entry = container_of (node, struct dpg_entry_dynamic, node);

  latch_lock (&entry->l);
  entry->rec_lsn = new_rec_lsn;
  latch_unlock (&entry->l);

  latch_unlock (&t->l);
}

u32
dpgt_dyn_get_size (struct dpg_table_dynamic *t)
{
  latch_lock (&t->l);
  u32 size = adptv_htable_size (&t->t);
  latch_unlock (&t->l);
  return size;
}

/* Helper for finding minimum recovery LSN */
struct min_lsn_ctx
{
  lsn min;
};

static void
find_min_lsn (struct hnode *node, void *ctx)
{
  struct min_lsn_ctx *c = ctx;
  struct dpg_entry_dynamic *entry = container_of (node, struct dpg_entry_dynamic, node);

  if (c->min == U64_MAX || entry->rec_lsn < c->min)
    {
      c->min = entry->rec_lsn;
    }
}

lsn
dpgt_dyn_min_rec_lsn (struct dpg_table_dynamic *t)
{
  struct min_lsn_ctx ctx = { .min = U64_MAX };

  latch_lock (&t->l);
  adptv_htable_foreach (&t->t, find_min_lsn, &ctx);
  latch_unlock (&t->l);

  return ctx.min;
}

/* Helper for foreach with user callback */
struct foreach_ctx
{
  void (*action) (struct dpg_entry_dynamic *, void *);
  void *user_ctx;
};

static void
foreach_adapter (struct hnode *node, void *ctx)
{
  struct foreach_ctx *c = ctx;
  struct dpg_entry_dynamic *entry = container_of (node, struct dpg_entry_dynamic, node);
  c->action (entry, c->user_ctx);
}

void
dpgt_dyn_foreach (struct dpg_table_dynamic *t,
                  void (*action) (struct dpg_entry_dynamic *, void *ctx),
                  void *ctx)
{
  struct foreach_ctx fctx = { .action = action, .user_ctx = ctx };

  latch_lock (&t->l);
  adptv_htable_foreach (&t->t, foreach_adapter, &fctx);
  latch_unlock (&t->l);
}

/* Helper for merge */
struct merge_ctx
{
  struct dpg_table_dynamic *dest;
  error *e;
  err_t err;
};

static void
merge_entry (struct dpg_entry_dynamic *entry, void *ctx)
{
  struct merge_ctx *c = ctx;

  if (c->err != SUCCESS)
    {
      return;
    }

  /* Try to add to destination */
  c->err = dpgt_dyn_add_or_update (c->dest, entry->pg, entry->rec_lsn, c->e);
}

#ifndef NTEST
err_t
dpgt_dyn_merge_into (
    struct dpg_table_dynamic *dest,
    struct dpg_table_dynamic *src,
    error *e)
{
  struct merge_ctx ctx = { .dest = dest, .e = e, .err = SUCCESS };

  /* Add all entries from src to dest */
  dpgt_dyn_foreach (src, merge_entry, &ctx);

  if (ctx.err != SUCCESS)
    {
      return ctx.err;
    }

  /* Clear src by removing all entries */
  latch_lock (&src->l);
  struct hnode *node;
  while ((node = adptv_htable_lookup (&src->t, NULL, NULL)) != NULL)
    {
      struct dpg_entry_dynamic *entry = container_of (node, struct dpg_entry_dynamic, node);

      err_t err = adptv_htable_delete (&node, &src->t, &entry->node,
                                       dpg_entry_equals, e);
      if (err != SUCCESS)
        {
          latch_unlock (&src->l);
          return err;
        }

      /* Free the entry */
      free (entry);
    }
  latch_unlock (&src->l);

  return SUCCESS;
}

/* Helper for equality check */
struct equals_ctx
{
  struct dpg_table_dynamic *other;
  bool equal;
};

static void
check_entry_equal (struct dpg_entry_dynamic *entry, void *ctx)
{
  struct equals_ctx *c = ctx;

  if (!c->equal)
    {
      return;
    }

  struct dpg_entry_dynamic other_entry;
  bool found = dpgt_dyn_get (&other_entry, c->other, entry->pg);

  if (!found || other_entry.rec_lsn != entry->rec_lsn)
    {
      c->equal = false;
    }
}

bool
dpgt_dyn_equals (struct dpg_table_dynamic *left, struct dpg_table_dynamic *right)
{
  /* Different sizes means not equal */
  u32 left_size = dpgt_dyn_get_size (left);
  u32 right_size = dpgt_dyn_get_size (right);

  if (left_size != right_size)
    {
      return false;
    }

  /* Check all entries in left exist in right with same values */
  struct equals_ctx ctx = { .other = right, .equal = true };
  dpgt_dyn_foreach (left, check_entry_equal, &ctx);

  return ctx.equal;
}

#endif

/* Serialization format:
 * [u32: count] [entry1] [entry2] ... [entryN]
 * Each entry: [pgno: u64] [rec_lsn: u64]
 */

u32
dpgt_dyn_get_serialize_size (struct dpg_table_dynamic *t)
{
  u32 count = dpgt_dyn_get_size (t);
  return sizeof (u32) + count * (sizeof (pgno) + sizeof (lsn));
}

/* Helper for serialization */
struct serialize_ctx
{
  u8 *dest;
  u32 offset;
};

static void
serialize_entry (struct dpg_entry_dynamic *entry, void *ctx)
{
  struct serialize_ctx *c = ctx;

  /* Write pgno */
  memcpy (c->dest + c->offset, &entry->pg, sizeof (pgno));
  c->offset += sizeof (pgno);

  /* Write rec_lsn */
  memcpy (c->dest + c->offset, &entry->rec_lsn, sizeof (lsn));
  c->offset += sizeof (lsn);
}

u32
dpgt_dyn_serialize (u8 *dest, u32 dlen, struct dpg_table_dynamic *t)
{
  u32 count = dpgt_dyn_get_size (t);
  u32 required = dpgt_dyn_get_serialize_size (t);

  assert (dlen >= required && "buffer too small for serialization");
  (void)dlen;

  /* Write count */
  memcpy (dest, &count, sizeof (u32));

  /* Write entries */
  struct serialize_ctx ctx = { .dest = dest, .offset = sizeof (u32) };
  dpgt_dyn_foreach (t, serialize_entry, &ctx);

  return required;
}

err_t
dpgt_dyn_deserialize (struct dpg_table_dynamic *dest,
                      const u8 *src,
                      u32 slen,
                      error *e)
{
  err_t err;

  if (slen < sizeof (u32))
    {
      return error_causef (e, ERR_CORRUPT, "buffer too small for count");
    }

  /* Read count */
  u32 count;
  memcpy (&count, src, sizeof (u32));
  u32 offset = sizeof (u32);

  u32 required = sizeof (u32) + count * (sizeof (pgno) + sizeof (lsn));
  if (slen < required)
    {
      return error_causef (e, ERR_CORRUPT, "buffer too small for entries");
    }

  /* Initialize empty table */
  err = dpgt_dyn_open (dest, e);
  if (err != SUCCESS)
    {
      return err;
    }

  /* Read entries */
  for (u32 i = 0; i < count; i++)
    {
      pgno pg;
      lsn rec_lsn;

      memcpy (&pg, src + offset, sizeof (pgno));
      offset += sizeof (pgno);

      memcpy (&rec_lsn, src + offset, sizeof (lsn));
      offset += sizeof (lsn);

      if (dpgt_dyn_add (dest, pg, rec_lsn, e))
        {
          dpgt_dyn_close (dest, e);
          return e->cause_code;
        }
    }

  return SUCCESS;
}

#ifndef NTEST
#include <numstore/test/testing.h>
#include <pthread.h>

/* Test Suite: Basic Operations */

TEST (TT_UNIT, dpgt_dyn_open_close_basic)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);
  test_assert (dpgt_dyn_get_size (&t) == 0);
  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_add_and_get)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  err_t result = dpgt_dyn_add (&t, 100, 50, &e);
  test_assert (result == SUCCESS);

  struct dpg_entry_dynamic entry;
  bool found = dpgt_dyn_get (&entry, &t, 100);
  test_assert (found);
  test_assert (entry.pg == 100);
  test_assert (entry.rec_lsn == 50);

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_add_duplicate)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t, 100, 50, &e), &e);

  /* Adding duplicate should fail */
  err_t result = dpgt_dyn_add (&t, 100, 60, &e);
  test_assert (result == ERR_INVALID_ARGUMENT);

  /* Original value should be unchanged */
  struct dpg_entry_dynamic entry;
  dpgt_dyn_get_expect (&entry, &t, 100);
  test_assert (entry.rec_lsn == 50);

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_get_nonexistent)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  struct dpg_entry_dynamic entry;
  bool found = dpgt_dyn_get (&entry, &t, 999);
  test_assert (!found);

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_exists)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  test_assert (!dpgt_dyn_exists (&t, 100));

  test_err_t_wrap (dpgt_dyn_add (&t, 100, 50, &e), &e);
  test_assert (dpgt_dyn_exists (&t, 100));

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_remove)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t, 100, 50, &e), &e);
  test_assert (dpgt_dyn_exists (&t, 100));

  bool exists;
  test_err_t_wrap (dpgt_dyn_remove (&exists, &t, 100, &e), &e);
  test_assert (exists);
  test_assert (!dpgt_dyn_exists (&t, 100));

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_remove_nonexistent)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  bool exists;
  test_err_t_wrap (dpgt_dyn_remove (&exists, &t, 999, &e), &e);
  test_assert (!exists);

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_remove_expect)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t, 100, 50, &e), &e);
  test_err_t_wrap (dpgt_dyn_remove_expect (&t, 100, &e), &e);
  test_assert (!dpgt_dyn_exists (&t, 100));

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_add_or_update_new)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  test_err_t_wrap (dpgt_dyn_add_or_update (&t, 100, 50, &e), &e);
  test_assert (dpgt_dyn_exists (&t, 100));

  struct dpg_entry_dynamic entry;
  dpgt_dyn_get_expect (&entry, &t, 100);
  test_assert (entry.rec_lsn == 50);

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_add_or_update_existing)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t, 100, 50, &e), &e);
  test_err_t_wrap (dpgt_dyn_add_or_update (&t, 100, 75, &e), &e);

  struct dpg_entry_dynamic entry;
  dpgt_dyn_get_expect (&entry, &t, 100);
  test_assert (entry.rec_lsn == 75);
  test_assert (dpgt_dyn_get_size (&t) == 1);

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

/* Test Suite: Update Operations */

TEST (TT_UNIT, dpgt_dyn_update)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t, 100, 50, &e), &e);
  dpgt_dyn_update (&t, 100, 100);

  struct dpg_entry_dynamic entry;
  dpgt_dyn_get_expect (&entry, &t, 100);
  test_assert (entry.rec_lsn == 100);

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_update_multiple)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t, 100, 50, &e), &e);
  test_err_t_wrap (dpgt_dyn_add (&t, 200, 60, &e), &e);

  dpgt_dyn_update (&t, 100, 150);
  dpgt_dyn_update (&t, 200, 160);

  struct dpg_entry_dynamic entry;
  dpgt_dyn_get_expect (&entry, &t, 100);
  test_assert (entry.rec_lsn == 150);

  dpgt_dyn_get_expect (&entry, &t, 200);
  test_assert (entry.rec_lsn == 160);

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

/* Test Suite: Size and Statistics */

TEST (TT_UNIT, dpgt_dyn_get_size)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  test_assert (dpgt_dyn_get_size (&t) == 0);

  test_err_t_wrap (dpgt_dyn_add (&t, 100, 50, &e), &e);
  test_assert (dpgt_dyn_get_size (&t) == 1);

  test_err_t_wrap (dpgt_dyn_add (&t, 200, 60, &e), &e);
  test_assert (dpgt_dyn_get_size (&t) == 2);

  bool exists;
  test_err_t_wrap (dpgt_dyn_remove (&exists, &t, 100, &e), &e);
  test_assert (dpgt_dyn_get_size (&t) == 1);

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_min_rec_lsn_empty)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  lsn min = dpgt_dyn_min_rec_lsn (&t);
  test_assert (min == U64_MAX);

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_min_rec_lsn_single)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t, 100, 50, &e), &e);

  lsn min = dpgt_dyn_min_rec_lsn (&t);
  test_assert (min == 50);

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_min_rec_lsn_multiple)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t, 100, 75, &e), &e);
  test_err_t_wrap (dpgt_dyn_add (&t, 200, 50, &e), &e);
  test_err_t_wrap (dpgt_dyn_add (&t, 300, 100, &e), &e);

  lsn min = dpgt_dyn_min_rec_lsn (&t);
  test_assert (min == 50);

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

/* Test Suite: Foreach Iteration */

struct count_ctx
{
  u32 count;
};

static void
count_entries (struct dpg_entry_dynamic *entry, void *ctx)
{
  struct count_ctx *c = ctx;
  c->count++;
  (void)entry;
}

TEST (TT_UNIT, dpgt_dyn_foreach_empty)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  struct count_ctx ctx = { .count = 0 };
  dpgt_dyn_foreach (&t, count_entries, &ctx);
  test_assert (ctx.count == 0);

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_foreach_multiple)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  for (pgno pg = 100; pg < 110; pg++)
    {
      test_err_t_wrap (dpgt_dyn_add (&t, pg, pg * 2, &e), &e);
    }

  struct count_ctx ctx = { .count = 0 };
  dpgt_dyn_foreach (&t, count_entries, &ctx);
  test_assert (ctx.count == 10);

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

/* Test Suite: Serialization */

TEST (TT_UNIT, dpgt_dyn_serialize_empty)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  u32 size = dpgt_dyn_get_serialize_size (&t);
  test_assert (size == sizeof (u32));

  u8 buffer[256];
  u32 written = dpgt_dyn_serialize (buffer, size, &t);
  test_assert (written == size);

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_serialize_deserialize)
{
  error e = error_create ();
  struct dpg_table_dynamic t1;
  test_err_t_wrap (dpgt_dyn_open (&t1, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t1, 100, 50, &e), &e);
  test_err_t_wrap (dpgt_dyn_add (&t1, 200, 75, &e), &e);
  test_err_t_wrap (dpgt_dyn_add (&t1, 300, 100, &e), &e);

  u32 size = dpgt_dyn_get_serialize_size (&t1);
  u8 *buffer = malloc (size);
  test_assert (buffer != NULL);

  u32 written = dpgt_dyn_serialize (buffer, size, &t1);
  test_assert (written == size);

  struct dpg_table_dynamic t2;
  test_err_t_wrap (dpgt_dyn_deserialize (&t2, buffer, size, &e), &e);

  test_assert (dpgt_dyn_get_size (&t2) == 3);
  test_assert (dpgt_dyn_exists (&t2, 100));
  test_assert (dpgt_dyn_exists (&t2, 200));
  test_assert (dpgt_dyn_exists (&t2, 300));

  struct dpg_entry_dynamic entry;
  dpgt_dyn_get_expect (&entry, &t2, 100);
  test_assert (entry.rec_lsn == 50);

  dpgt_dyn_get_expect (&entry, &t2, 200);
  test_assert (entry.rec_lsn == 75);

  dpgt_dyn_get_expect (&entry, &t2, 300);
  test_assert (entry.rec_lsn == 100);

  free (buffer);
  test_err_t_wrap (dpgt_dyn_close (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_close (&t2, &e), &e);
}

/* Test Suite: Merge Operations */

TEST (TT_UNIT, dpgt_dyn_merge_empty_to_empty)
{
  error e = error_create ();
  struct dpg_table_dynamic t1, t2;
  test_err_t_wrap (dpgt_dyn_open (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_open (&t2, &e), &e);

  test_err_t_wrap (dpgt_dyn_merge_into (&t1, &t2, &e), &e);

  test_assert (dpgt_dyn_get_size (&t1) == 0);
  test_assert (dpgt_dyn_get_size (&t2) == 0);

  test_err_t_wrap (dpgt_dyn_close (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_close (&t2, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_merge_to_empty)
{
  error e = error_create ();
  struct dpg_table_dynamic t1, t2;
  test_err_t_wrap (dpgt_dyn_open (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_open (&t2, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t2, 100, 50, &e), &e);
  test_err_t_wrap (dpgt_dyn_add (&t2, 200, 75, &e), &e);

  test_err_t_wrap (dpgt_dyn_merge_into (&t1, &t2, &e), &e);

  test_assert (dpgt_dyn_get_size (&t1) == 2);
  test_assert (dpgt_dyn_get_size (&t2) == 0);
  test_assert (dpgt_dyn_exists (&t1, 100));
  test_assert (dpgt_dyn_exists (&t1, 200));

  test_err_t_wrap (dpgt_dyn_close (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_close (&t2, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_merge_both_nonempty)
{
  error e = error_create ();
  struct dpg_table_dynamic t1, t2;
  test_err_t_wrap (dpgt_dyn_open (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_open (&t2, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t1, 100, 50, &e), &e);
  test_err_t_wrap (dpgt_dyn_add (&t2, 200, 75, &e), &e);
  test_err_t_wrap (dpgt_dyn_add (&t2, 300, 100, &e), &e);

  test_err_t_wrap (dpgt_dyn_merge_into (&t1, &t2, &e), &e);

  test_assert (dpgt_dyn_get_size (&t1) == 3);
  test_assert (dpgt_dyn_get_size (&t2) == 0);
  test_assert (dpgt_dyn_exists (&t1, 100));
  test_assert (dpgt_dyn_exists (&t1, 200));
  test_assert (dpgt_dyn_exists (&t1, 300));

  test_err_t_wrap (dpgt_dyn_close (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_close (&t2, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_merge_with_update)
{
  error e = error_create ();
  struct dpg_table_dynamic t1, t2;
  test_err_t_wrap (dpgt_dyn_open (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_open (&t2, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t1, 100, 50, &e), &e);
  test_err_t_wrap (dpgt_dyn_add (&t2, 100, 75, &e), &e);

  test_err_t_wrap (dpgt_dyn_merge_into (&t1, &t2, &e), &e);

  test_assert (dpgt_dyn_get_size (&t1) == 1);
  test_assert (dpgt_dyn_get_size (&t2) == 0);

  struct dpg_entry_dynamic entry;
  dpgt_dyn_get_expect (&entry, &t1, 100);
  test_assert (entry.rec_lsn == 75);

  test_err_t_wrap (dpgt_dyn_close (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_close (&t2, &e), &e);
}

/* Test Suite: Equality */

TEST (TT_UNIT, dpgt_dyn_equals_empty)
{
  error e = error_create ();
  struct dpg_table_dynamic t1, t2;
  test_err_t_wrap (dpgt_dyn_open (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_open (&t2, &e), &e);

  test_assert (dpgt_dyn_equals (&t1, &t2));

  test_err_t_wrap (dpgt_dyn_close (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_close (&t2, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_equals_same_content)
{
  error e = error_create ();
  struct dpg_table_dynamic t1, t2;
  test_err_t_wrap (dpgt_dyn_open (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_open (&t2, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t1, 100, 50, &e), &e);
  test_err_t_wrap (dpgt_dyn_add (&t1, 200, 75, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t2, 100, 50, &e), &e);
  test_err_t_wrap (dpgt_dyn_add (&t2, 200, 75, &e), &e);

  test_assert (dpgt_dyn_equals (&t1, &t2));

  test_err_t_wrap (dpgt_dyn_close (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_close (&t2, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_equals_different_size)
{
  error e = error_create ();
  struct dpg_table_dynamic t1, t2;
  test_err_t_wrap (dpgt_dyn_open (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_open (&t2, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t1, 100, 50, &e), &e);
  test_err_t_wrap (dpgt_dyn_add (&t2, 100, 50, &e), &e);
  test_err_t_wrap (dpgt_dyn_add (&t2, 200, 75, &e), &e);

  test_assert (!dpgt_dyn_equals (&t1, &t2));

  test_err_t_wrap (dpgt_dyn_close (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_close (&t2, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_equals_different_values)
{
  error e = error_create ();
  struct dpg_table_dynamic t1, t2;
  test_err_t_wrap (dpgt_dyn_open (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_open (&t2, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t1, 100, 50, &e), &e);
  test_err_t_wrap (dpgt_dyn_add (&t2, 100, 75, &e), &e);

  test_assert (!dpgt_dyn_equals (&t1, &t2));

  test_err_t_wrap (dpgt_dyn_close (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_close (&t2, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_equals_different_keys)
{
  error e = error_create ();
  struct dpg_table_dynamic t1, t2;
  test_err_t_wrap (dpgt_dyn_open (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_open (&t2, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t1, 100, 50, &e), &e);
  test_err_t_wrap (dpgt_dyn_add (&t2, 200, 50, &e), &e);

  test_assert (!dpgt_dyn_equals (&t1, &t2));

  test_err_t_wrap (dpgt_dyn_close (&t1, &e), &e);
  test_err_t_wrap (dpgt_dyn_close (&t2, &e), &e);
}

/* Test Suite: Edge Cases */

TEST (TT_UNIT, dpgt_dyn_double_remove)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t, 100, 50, &e), &e);

  bool exists;
  test_err_t_wrap (dpgt_dyn_remove (&exists, &t, 100, &e), &e);
  test_assert (exists);

  test_err_t_wrap (dpgt_dyn_remove (&exists, &t, 100, &e), &e);
  test_assert (!exists);

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_operations_after_remove)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t, 100, 50, &e), &e);
  bool exists;
  test_err_t_wrap (dpgt_dyn_remove (&exists, &t, 100, &e), &e);

  /* Can add again after remove */
  test_err_t_wrap (dpgt_dyn_add (&t, 100, 75, &e), &e);

  struct dpg_entry_dynamic entry;
  dpgt_dyn_get_expect (&entry, &t, 100);
  test_assert (entry.rec_lsn == 75);

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

TEST (TT_UNIT, dpgt_dyn_large_number_of_entries)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  /* Add 1000 entries */
  for (pgno pg = 0; pg < 1000; pg++)
    {
      test_err_t_wrap (dpgt_dyn_add (&t, pg, pg * 10, &e), &e);
    }

  test_assert (dpgt_dyn_get_size (&t) == 1000);

  /* Verify all entries */
  for (pgno pg = 0; pg < 1000; pg++)
    {
      struct dpg_entry_dynamic entry;
      dpgt_dyn_get_expect (&entry, &t, pg);
      test_assert (entry.pg == pg);
      test_assert (entry.rec_lsn == pg * 10);
    }

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

/* Test Suite: Concurrent Operations */

struct thread_insert_ctx
{
  struct dpg_table_dynamic *t;
  pgno start_pg;
  u32 count;
};

static void *
thread_insert_func (void *arg)
{
  struct thread_insert_ctx *ctx = arg;
  error e = error_create ();

  for (u32 i = 0; i < ctx->count; i++)
    {
      pgno pg = ctx->start_pg + i;
      dpgt_dyn_add (ctx->t, pg, pg * 2, &e);
    }

  return NULL;
}

TEST (TT_UNIT, dpgt_dyn_concurrent_inserts)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  const int num_threads = 3;
  const u32 entries_per_thread = 100;
  pthread_t threads[num_threads];
  struct thread_insert_ctx contexts[num_threads];

  /* Spawn threads */
  for (int i = 0; i < num_threads; i++)
    {
      contexts[i].t = &t;
      contexts[i].start_pg = i * entries_per_thread;
      contexts[i].count = entries_per_thread;
      pthread_create (&threads[i], NULL, thread_insert_func, &contexts[i]);
    }

  /* Wait for completion */
  for (int i = 0; i < num_threads; i++)
    {
      pthread_join (threads[i], NULL);
    }

  /* Verify all entries */
  test_assert (dpgt_dyn_get_size (&t) == num_threads * entries_per_thread);

  for (int i = 0; i < num_threads; i++)
    {
      for (u32 j = 0; j < entries_per_thread; j++)
        {
          pgno pg = i * entries_per_thread + j;
          test_assert (dpgt_dyn_exists (&t, pg));
        }
    }

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

struct thread_read_ctx
{
  struct dpg_table_dynamic *t;
  pgno start_pg;
  pgno end_pg;
  bool success;
};

static void *
thread_read_func (void *arg)
{
  struct thread_read_ctx *ctx = arg;
  ctx->success = true;

  for (pgno pg = ctx->start_pg; pg < ctx->end_pg; pg++)
    {
      struct dpg_entry_dynamic entry;
      bool found = dpgt_dyn_get (&entry, ctx->t, pg);
      if (found && entry.rec_lsn != pg * 2)
        {
          ctx->success = false;
          break;
        }
    }

  return NULL;
}

TEST (TT_UNIT, dpgt_dyn_concurrent_reads)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  /* Pre-populate */
  for (pgno pg = 0; pg < 300; pg++)
    {
      test_err_t_wrap (dpgt_dyn_add (&t, pg, pg * 2, &e), &e);
    }

  const int num_threads = 3;
  pthread_t threads[num_threads];
  struct thread_read_ctx contexts[num_threads];

  /* Spawn readers with overlapping ranges */
  for (int i = 0; i < num_threads; i++)
    {
      contexts[i].t = &t;
      contexts[i].start_pg = i * 50;
      contexts[i].end_pg = (i + 1) * 100;
      pthread_create (&threads[i], NULL, thread_read_func, &contexts[i]);
    }

  /* Wait for completion */
  for (int i = 0; i < num_threads; i++)
    {
      pthread_join (threads[i], NULL);
      test_assert (contexts[i].success);
    }

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}

struct thread_update_ctx
{
  struct dpg_table_dynamic *t;
  pgno pg;
  u32 iterations;
};

static void *
thread_update_func (void *arg)
{
  struct thread_update_ctx *ctx = arg;

  for (u32 i = 0; i < ctx->iterations; i++)
    {
      dpgt_dyn_update (ctx->t, ctx->pg, i);
    }

  return NULL;
}

TEST (TT_UNIT, dpgt_dyn_concurrent_updates)
{
  error e = error_create ();
  struct dpg_table_dynamic t;
  test_err_t_wrap (dpgt_dyn_open (&t, &e), &e);

  test_err_t_wrap (dpgt_dyn_add (&t, 100, 0, &e), &e);

  const int num_threads = 3;
  const u32 iterations = 100;
  pthread_t threads[num_threads];
  struct thread_update_ctx contexts[num_threads];

  /* Spawn updaters */
  for (int i = 0; i < num_threads; i++)
    {
      contexts[i].t = &t;
      contexts[i].pg = 100;
      contexts[i].iterations = iterations;
      pthread_create (&threads[i], NULL, thread_update_func, &contexts[i]);
    }

  /* Wait for completion */
  for (int i = 0; i < num_threads; i++)
    {
      pthread_join (threads[i], NULL);
    }

  /* Entry should still exist and have some valid LSN */
  test_assert (dpgt_dyn_exists (&t, 100));

  test_err_t_wrap (dpgt_dyn_close (&t, &e), &e);
}
#endif
