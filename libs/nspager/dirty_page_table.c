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
 *   Implements dirty_page_table.h. Manages dirty page tracking with hash table for efficient lookups during transaction commit.
 */

#include "numstore/core/adptv_hash_table.h"
#include "numstore/core/hash_table.h"
#include "numstore/core/slab_alloc.h"
#include <numstore/pager/dirty_page_table.h>

#include <numstore/core/assert.h>
#include <numstore/core/bytes.h>
#include <numstore/core/clock_allocator.h>
#include <numstore/core/deserializer.h>
#include <numstore/core/error.h>
#include <numstore/core/ht_models.h>
#include <numstore/core/latch.h>
#include <numstore/core/random.h>
#include <numstore/core/serializer.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/types.h>
#include <numstore/test/testing.h>

#include <config.h>

struct dpg_entry
{
  lsn rec_lsn;
  pgno pg;
  struct hnode node;
  struct latch l;
};

#define DPGT_SERIAL_UNIT (sizeof (pgno) + sizeof (lsn))

static err_t
dpge_key_init (struct dpg_entry *dest, pgno pg, error *e)
{
  dest->pg = pg;
  err_t ret = latch_init (&dest->l, e);
  if (ret < SUCCESS)
    {
      return ret;
    }
  hnode_init (&dest->node, pg);
  return SUCCESS;
}

static err_t
dpge_init (struct dpg_entry *dest, pgno pg, lsn rec_lsn, error *e)
{
  dest->pg = pg;
  dest->rec_lsn = rec_lsn;
  err_t ret = latch_init (&dest->l, e);
  if (ret < SUCCESS)
    {
      return ret;
    }
  hnode_init (&dest->node, pg);
  return SUCCESS;
}

static bool
dpge_equals (const struct hnode *left, const struct hnode *right)
{
  // Might have passed the exact same reference as exists in the htable
  if (left == right)
    {
      return true;
    }

  // Otherwise, passed a key with just relevant information
  else
    {
      struct dpg_entry *_left = container_of (left, struct dpg_entry, node);
      struct dpg_entry *_right = container_of (right, struct dpg_entry, node);

      latch_lock (&_left->l);
      latch_lock (&_right->l);

      bool ret = _left->pg == _right->pg;

      latch_unlock (&_right->l);
      latch_unlock (&_left->l);

      return ret;
    }
}

DEFINE_DBG_ASSERT (
    struct dpg_table, dirty_pg_table, d, {
      ASSERT (d);
    })

// Lifecycle
err_t
dpgt_open (struct dpg_table *dest, error *e)
{
  slab_alloc_init (&dest->alloc, sizeof (struct dpg_entry), 1000);

  struct adptv_htable_settings settings = {
    .max_load_factor = 8,
    .min_load_factor = 1,
    .rehashing_work = 28,
    .max_size = 2048,
    .min_size = 10,
  };

  err_t_wrap (adptv_htable_init (&dest->table, settings, e), e);
  err_t_wrap (latch_init (&dest->l, e), e);

  return SUCCESS;
}

void
dpgt_close (struct dpg_table *t)
{
  DBG_ASSERT (dirty_pg_table, t);
  latch_lock (&t->l);

  slab_alloc_destroy (&t->alloc);
  adptv_htable_free (&t->table);

  latch_unlock (&t->l);
}

static void
i_log_dpge_in_dpgt (struct hnode *node, void *_log_level)
{
  int *log_level = _log_level;

  struct dpg_entry *entry = container_of (node, struct dpg_entry, node);

  latch_lock (&entry->l);
  i_printf (*log_level, "|pg = %10" PRpgno " rec_lsn = %10" PRlsn "|\n", entry->pg, entry->rec_lsn);
  latch_unlock (&entry->l);
}

void
i_log_dpgt (int log_level, struct dpg_table *dpt)
{
  latch_lock (&dpt->l);

  i_log (log_level, "================ Dirty Page Table START ================\n");
  adptv_htable_foreach (&dpt->table, i_log_dpge_in_dpgt, &log_level);
  i_log (log_level, "================ Dirty Page Table END ================\n");

  latch_unlock (&dpt->l);
}

struct merge_ctx
{
  struct dpg_table *dest;
  error *e;
};

static void
merge_dpge (pgno pg, lsn rec_lsn, void *vctx)
{
  struct merge_ctx *ctx = vctx;

  if (ctx->e->cause_code)
    {
      return;
    }

  if (dpgt_add (ctx->dest, pg, rec_lsn, ctx->e))
    {
      return;
    }
}

err_t
dpgt_merge_into (struct dpg_table *dest, struct dpg_table *src, error *e)
{
  struct merge_ctx ctx = {
    .dest = dest,
    .e = e,
  };

  latch_lock (&src->l);
  dpgt_foreach (src, merge_dpge, &ctx);
  latch_unlock (&src->l);

  return ctx.e->cause_code;
}

static void
dpge_max (pgno pg, lsn rec_lsn, void *ctx)
{
  lsn *min = ctx;

  if (rec_lsn < *min)
    {
      *min = rec_lsn;
    }
}

lsn
dpgt_min_rec_lsn (struct dpg_table *d)
{
  lsn min;

  latch_lock (&d->l);
  dpgt_foreach (d, dpge_max, &min);
  latch_unlock (&d->l);

  return min;
}

struct foreach_ctx
{
  void (*action) (pgno pg, lsn rec_lsn, void *ctx);
  void *ctx;
};

static void
hnode_foreach (struct hnode *node, void *ctx)
{
  struct foreach_ctx *_ctx = ctx;
  struct dpg_entry *entry = container_of (node, struct dpg_entry, node);

  pgno pg;
  lsn l;

  latch_lock (&entry->l);

  pg = entry->pg;
  l = entry->rec_lsn;

  latch_unlock (&entry->l);

  _ctx->action (pg, l, _ctx->ctx);
}

void
dpgt_foreach (struct dpg_table *t, void (*action) (pgno pg, lsn rec_lsn, void *ctx), void *ctx)
{
  struct foreach_ctx _ctx = {
    .action = action,
    .ctx = ctx,
  };
  adptv_htable_foreach (&t->table, hnode_foreach, &_ctx);
}

u32
dpgt_get_size (struct dpg_table *d)
{
  return adptv_htable_size (&d->table);
}

bool
dpgt_exists (struct dpg_table *t, pgno pg, error *e)
{
  struct dpg_entry entry;
  if (dpge_key_init (&entry, pg, e) < SUCCESS)
    {
      return false;
    }

  struct hnode *ret = adptv_htable_lookup (&t->table, &entry.node, dpge_equals);

  return ret != NULL;
}

err_t
dpgt_add (struct dpg_table *t, pgno pg, lsn rec_lsn, error *e)
{
  DBG_ASSERT (dirty_pg_table, t);

  latch_lock (&t->l);

  struct dpg_entry *v = slab_alloc_alloc (&t->alloc, e);
  if (v == NULL)
    {
      goto theend;
    }

  if (dpge_init (v, pg, rec_lsn, e) < SUCCESS)
    {
      goto theend;
    }

  if (adptv_htable_insert (&t->table, &v->node, e))
    {
      goto theend;
    }

theend:
  latch_unlock (&t->l);
  return e->cause_code;
}

bool
dpgt_get (lsn *dest, struct dpg_table *t, pgno pg, error *e)
{
  DBG_ASSERT (dirty_pg_table, t);

  struct dpg_entry key;
  if (dpge_key_init (&key, pg, e) < SUCCESS)
    {
      return false;
    }

  latch_lock (&t->l);

  struct hnode *node = adptv_htable_lookup (&t->table, &key.node, dpge_equals);
  if (node)
    {
      *dest = container_of (node, struct dpg_entry, node)->rec_lsn;
    }

  latch_unlock (&t->l);

  return node != NULL;
}

err_t
dpgt_get_expect (lsn *dest, struct dpg_table *t, pgno pg, error *e)
{
  DBG_ASSERT (dirty_pg_table, t);

  struct dpg_entry key;
  err_t_wrap (dpge_key_init (&key, pg, e), e);

  latch_lock (&t->l);

  struct hnode *node = adptv_htable_lookup (&t->table, &key.node, dpge_equals);
  ASSERT (node != NULL);
  *dest = container_of (node, struct dpg_entry, node)->rec_lsn;

  latch_unlock (&t->l);
  return SUCCESS;
}

err_t
dpgt_remove (bool *exists, struct dpg_table *t, pgno pg, error *e)
{
  DBG_ASSERT (dirty_pg_table, t);

  struct dpg_entry key;
  err_t_wrap (dpge_key_init (&key, pg, e), e);

  latch_lock (&t->l);

  struct hnode *node = adptv_htable_lookup (&t->table, &key.node, dpge_equals);

  if (node == NULL)
    {
      *exists = false;
    }

  if (adptv_htable_delete (NULL, &t->table, node, dpge_equals, e))
    {
      goto theend;
    }

theend:
  latch_unlock (&t->l);

  return e->cause_code;
}

err_t
dpgt_remove_expect (struct dpg_table *t, pgno pg, error *e)
{
  DBG_ASSERT (dirty_pg_table, t);

  struct dpg_entry key;
  err_t_wrap (dpge_key_init (&key, pg, e), e);

  latch_lock (&t->l);

  struct hnode *node = adptv_htable_lookup (&t->table, &key.node, dpge_equals);
  ASSERTF (node != NULL, "Expected page: %" PRpgno " to be dirty, but wasn't in the dpgt\n", pg);
  if (adptv_htable_delete (NULL, &t->table, node, dpge_equals, e))
    {
      goto theend;
    }

theend:
  latch_unlock (&t->l);

  return e->cause_code;
}

err_t
dpgt_update (struct dpg_table *t, pgno pg, lsn new_rec_lsn, error *e)
{
  struct dpg_entry key;
  err_t_wrap (dpge_key_init (&key, pg, e), e);

  latch_lock (&t->l);
  {
    DBG_ASSERT (dirty_pg_table, t);

    struct hnode *node = adptv_htable_lookup (&t->table, &key.node, dpge_equals);
    ASSERT (node != NULL);
    struct dpg_entry *entry = container_of (node, struct dpg_entry, node);

    latch_lock (&entry->l);
    {
      entry->rec_lsn = new_rec_lsn;
    }
    latch_unlock (&entry->l);
  }

  latch_unlock (&t->l);
  return SUCCESS;
}

u32
dpgt_get_serialize_size (struct dpg_table *t)
{
  return adptv_htable_size (&t->table) * DPGT_SERIAL_UNIT;
}

struct dpge_serialize_ctx
{
  struct serializer s;
};

static void
hnode_foreach_serialize (struct hnode *node, void *ctx)
{
  struct dpge_serialize_ctx *_ctx = ctx;

  struct dpg_entry *entry = container_of (node, struct dpg_entry, node);

  pgno pg;
  lsn rec_lsn;

  latch_lock (&entry->l);

  pg = entry->pg;
  rec_lsn = entry->pg;

  latch_unlock (&entry->l);

  srlizr_write_expect (&_ctx->s, &pg, sizeof (pg));
  srlizr_write_expect (&_ctx->s, &rec_lsn, sizeof (rec_lsn));
}

u32
dpgt_serialize (u8 *dest, u32 dlen, struct dpg_table *t)
{
  struct dpge_serialize_ctx ctx = {
    .s = srlizr_create (dest, dlen),
  };

  adptv_htable_foreach (&t->table, hnode_foreach_serialize, &ctx);

  return ctx.s.dlen;
}

err_t
dpgt_deserialize (struct dpg_table *dest, const u8 *src, u32 slen, error *e)
{
  if (dpgt_open (dest, e))
    {
      return e->cause_code;
    }

  if (slen == 0)
    {
      return SUCCESS;
    }

  struct deserializer d = dsrlizr_create (src, slen);

  ASSERT (slen % DPGT_SERIAL_UNIT == 0);
  u32 tlen = slen / DPGT_SERIAL_UNIT;

  for (u32 i = 0; i < tlen; ++i)
    {
      pgno pg;
      lsn rec_lsn;

      dsrlizr_read_expect (&pg, sizeof (pg), &d);
      dsrlizr_read_expect (&rec_lsn, sizeof (rec_lsn), &d);

      if (dpgt_add (dest, pg, rec_lsn, e))
        {
          goto failed;
        }
    }

  return SUCCESS;

failed:
  dpgt_close (dest);
  return e->cause_code;
}

u32
dpgtlen_from_serialized (u32 slen)
{
  ASSERT (slen % DPGT_SERIAL_UNIT == 0);
  return slen / DPGT_SERIAL_UNIT;
}

#ifndef NTEST

struct dpgt_eq_ctx
{
  struct dpg_table *other;
  bool ret;
  error *e;
};

static void
dpgt_eq_foreach (struct hnode *node, void *_ctx)
{
  struct dpgt_eq_ctx *ctx = _ctx;
  if (ctx->ret == false)
    {
      return;
    }

  struct dpg_entry *entry = container_of (node, struct dpg_entry, node);
  struct dpg_entry candidate;

  latch_lock (&entry->l);
  {
    if (dpge_key_init (&candidate, entry->pg, ctx->e) < SUCCESS)
      {
        ctx->ret = false;
        latch_unlock (&entry->l);
        return;
      }

    struct hnode *other_node = adptv_htable_lookup (&ctx->other->table, &candidate.node, dpge_equals);

    if (other_node == NULL)
      {
        ctx->ret = false;
        latch_unlock (&entry->l);
        return;
      }

    struct dpg_entry *other = container_of (other_node, struct dpg_entry, node);

    latch_lock (&other->l);
    {
      ASSERT (other->pg == entry->pg);
      ctx->ret = other->rec_lsn == entry->rec_lsn;
    }
    latch_unlock (&other->l);
  }
  latch_unlock (&entry->l);
}

bool
dpgt_equal (struct dpg_table *left, struct dpg_table *right, error *e)
{
  latch_lock (&left->l);
  latch_lock (&right->l);

  if (adptv_htable_size (&left->table) != adptv_htable_size (&right->table))
    {
      latch_unlock (&right->l);
      latch_unlock (&left->l);
      return false;
    }

  struct dpgt_eq_ctx ctx = {
    .other = right,
    .ret = true,
    .e = e,
  };
  adptv_htable_foreach (&left->table, dpgt_eq_foreach, &ctx);

  latch_unlock (&right->l);
  latch_unlock (&left->l);

  return ctx.ret;
}

err_t
dpgt_rand_populate (struct dpg_table *t, error *e)
{
  latch_lock (&t->l);
  u32 len = adptv_htable_size (&t->table);

  pgno pg = 0;
  lsn l = 0;

  for (u32 i = 0; i < 1000 - len; ++i, pg += randu32r (0, 100), l += randu32r (0, 100))
    {
      if (dpgt_add (t, pg, l, e))
        {
          goto theend;
        }
    }

theend:
  latch_unlock (&t->l);
  return e->cause_code;
}

void
dpgt_crash (struct dpg_table *t)
{
  DBG_ASSERT (dirty_pg_table, t);
  adptv_htable_free (&t->table);
  slab_alloc_destroy (&t->alloc);
}
#endif
