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

#include <numstore/pager/dirty_page_table.h>

#include <numstore/core/assert.h>
#include <numstore/core/bytes.h>
#include <numstore/core/clock_allocator.h>
#include <numstore/core/deserializer.h>
#include <numstore/core/error.h>
#include <numstore/core/ht_models.h>
#include <numstore/core/random.h>
#include <numstore/core/serializer.h>
#include <numstore/core/spx_latch.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/types.h>
#include <numstore/test/testing.h>

#include <config.h>

DEFINE_DBG_ASSERT (
    struct dpg_table, dirty_pg_table, d, {
      ASSERT (d);
    })

// Lifecycle
struct dpg_table *
dpgt_open (error *e)
{
  struct dpg_table *ret = i_calloc (1, sizeof *ret, e);
  if (ret == NULL)
    {
      return NULL;
    }

  ht_init_dpt (&ret->table, ret->_table, arrlen (ret->_table));

  err_t result = clck_alloc_open (&ret->alloc, sizeof (struct dpg_entry), arrlen (ret->_table), e);
  if (result != SUCCESS)
    {
      i_free (ret);
      return NULL;
    }

  spx_latch_init (&ret->l);

  return ret;
}

void
dpgt_close (struct dpg_table *t)
{
  DBG_ASSERT (dirty_pg_table, t);
  clck_alloc_close (&t->alloc);
  i_free (t);
}

bool
dpgt_equal (struct dpg_table *left, struct dpg_table *right)
{
  u32 left_len = 0;
  bool ret = false;

  spx_latch_lock_s (&left->l);
  spx_latch_lock_s (&right->l);

  for (u32 i = 0; i < arrlen (left->_table); ++i)
    {
      hentry_dpt entry = left->_table[i];

      if (entry.present)
        {
          left_len++;

          hdata_dpt dpe = entry.data;

          hdata_dpt rcand;
          switch (ht_get_dpt (&right->table, &rcand, dpe.key))
            {
            case HTAR_SUCCESS:
              {
                bool equal = rcand.value->pg == dpe.value->pg;
                equal = equal && rcand.value->rec_lsn == dpe.value->rec_lsn;
                if (!equal)
                  {
                    goto theend;
                  }
                break;
              }
            case HTAR_DOESNT_EXIST:
              {
                goto theend;
              }
            }
        }
    }

  u32 right_len = 0;
  for (u32 i = 0; i < arrlen (right->_table); ++i)
    {
      hentry_dpt entry = right->_table[i];

      if (entry.present)
        {
          right_len++;
        }
    }

  ret = left_len == right_len;

theend:
  spx_latch_unlock_s (&left->l);
  spx_latch_unlock_s (&right->l);

  return ret;
}

void
dpgt_rand_populate (struct dpg_table *dpt)
{
  u32 len = 0;

  for (u32 i = 0; i < arrlen (dpt->_table); i++)
    {
      if (dpt->_table[i].present)
        {
          len++;
        }
    }

  len = randu32r (0, arrlen (dpt->_table) - len);

  for (u32 i = 0; i < len; ++i)
    {
      if (dpgt_add (dpt, i, randu32r (0, 100), NULL))
        {
          return;
        }
    }
}

void
i_log_dpgt (int log_level, struct dpg_table *dpt)
{
  spx_latch_lock_s (&dpt->l);

  i_log (log_level, "================ Dirty Page Table START ================\n");
  i_printf (log_level, "txid recLSN\n");
  for (u32 i = 0; i < arrlen (dpt->_table); ++i)
    {
      const hentry_dpt e = dpt->_table[i];

      if (e.present)
        {
          spx_latch_lock_s (&e.data.value->l);

          i_printf (log_level, "%" PRpgno " %" PRspgno "\n", e.data.value->pg, e.data.value->rec_lsn);

          spx_latch_unlock_s (&e.data.value->l);
        }
    }
  i_log (log_level, "================ Dirty Page Table END ================\n");

  spx_latch_unlock_s (&dpt->l);
}

// Methods
err_t
dpgt_add (struct dpg_table *t, pgno pg, lsn rec_lsn, error *e)
{
  DBG_ASSERT (dirty_pg_table, t);

  spx_latch_lock_x (&t->l);

  struct dpg_entry *v = clck_alloc_alloc (&t->alloc, e);
  if (v == NULL)
    {
      spx_latch_unlock_x (&t->l);
      return error_change_causef (e, ERR_DPGT_FULL, "Not enough space in the dirty page table");
    }

  hdata_dpt data = {
    .key = pg,
    .value = v,
  };

  // Expect because clock allocator and txn table have the same size

  ht_insert_expect_dpt (&t->table, data);

  *v = (struct dpg_entry){
    .rec_lsn = rec_lsn,
    .pg = pg,
  };
  spx_latch_init (&v->l);
  spx_latch_unlock_x (&t->l);

  return SUCCESS;
}

bool
dpe_get (struct dpg_entry *dest, struct dpg_table *t, pgno pg)
{
  DBG_ASSERT (dirty_pg_table, t);

  hdata_dpt _dest;

  spx_latch_lock_s (&t->l);

  hta_res res = ht_get_dpt (&t->table, &_dest, pg);

  switch (res)
    {
    case HTAR_DOESNT_EXIST:
      {
        spx_latch_unlock_s (&t->l);
        return false;
      }
    case HTAR_SUCCESS:
      {
        spx_latch_lock_s (&_dest.value->l);
        *dest = *_dest.value;
        spx_latch_unlock_s (&_dest.value->l);

        spx_latch_unlock_s (&t->l);
        return true;
      }
    }
  return false;
}

struct dpg_entry
dpgt_get_expect (struct dpg_table *t, pgno pg)
{
  DBG_ASSERT (dirty_pg_table, t);

  spx_latch_lock_s (&t->l);

  hdata_dpt dest;
  ht_get_expect_dpt (&t->table, &dest, pg);

  spx_latch_lock_s (&dest.value->l);
  struct dpg_entry ret = *dest.value;
  spx_latch_unlock_s (&dest.value->l);

  spx_latch_unlock_s (&t->l);

  return ret;
}

void
dpgt_update (struct dpg_table *d, pgno pg, lsn new_rec_lsn)
{
  spx_latch_lock_s (&d->l);

  hdata_dpt dest;
  ht_get_expect_dpt (&d->table, &dest, pg);

  spx_latch_lock_x (&dest.value->l);
  dest.value->rec_lsn = new_rec_lsn;
  spx_latch_unlock_x (&dest.value->l);

  spx_latch_unlock_s (&d->l);
}

bool
dpgt_remove (struct dpg_table *t, pgno pg)
{
  DBG_ASSERT (dirty_pg_table, t);

  spx_latch_lock_x (&t->l);

  hdata_dpt dest;
  hta_res res = ht_delete_dpt (&t->table, &dest, pg);
  switch (res)
    {
    case HTAR_SUCCESS:
      {
        clck_alloc_free (&t->alloc, dest.value);
        spx_latch_unlock_x (&t->l);
        return true;
      }
    case HTAR_DOESNT_EXIST:
      {
        spx_latch_unlock_x (&t->l);
        return false;
      }
    }
  UNREACHABLE ();
}

void
dpgt_remove_expect (struct dpg_table *t, pgno pg)
{
  hdata_dpt dest;
  spx_latch_lock_x (&t->l);
  ht_delete_expect_dpt (&t->table, &dest, pg);
  clck_alloc_free (&t->alloc, dest.value);
  spx_latch_unlock_x (&t->l);
}

lsn
dpgt_min_rec_lsn (struct dpg_table *d)
{
  spx_latch_lock_s (&d->l);

  lsn ret = (lsn)-1;
  for (p_size i = 0; i < arrlen (d->_table); ++i)
    {
      hentry_dpt *entry = &d->_table[i];
      if (entry->present)
        {
          spx_latch_lock_s (&entry->data.value->l);
          lsn rec_lsn = entry->data.value->rec_lsn;
          spx_latch_unlock_s (&entry->data.value->l);

          if (rec_lsn < ret)
            {
              ret = rec_lsn;
            }
        }
    }

  spx_latch_unlock_s (&d->l);

  return ret;
}

void
dpgt_merge_into (struct dpg_table *dest, struct dpg_table *src)
{
  spx_latch_lock_s (&src->l);
  spx_latch_lock_x (&dest->l);

  for (p_size i = 0; i < arrlen (src->_table); ++i)
    {
      hentry_dpt *entry = &src->_table[i];
      if (entry->present)
        {
          hdata_dpt data;
          switch (ht_delete_dpt (&dest->table, &data, entry->data.key))
            {
            case HTAR_DOESNT_EXIST:
              {
                ht_insert_expect_dpt (&dest->table, entry->data);
                break;
              }
            case HTAR_SUCCESS:
              {
                ht_insert_expect_dpt (&dest->table, entry->data);
                break;
              }
            }
        }
    }

  spx_latch_unlock_x (&dest->l);
  spx_latch_unlock_s (&src->l);
}

//////////////////////////////////////////////////
///// Serialization / Deserialization for checkpoint

u32
dpgt_serialize (u8 dest[MAX_DPGT_SRL_SIZE], struct dpg_table *t)
{
  struct serializer s = srlizr_create (dest, MAX_DPGT_SRL_SIZE);

  spx_latch_lock_s (&t->l);

  for (u32 i = 0; i < arrlen (t->_table); ++i)
    {
      if (t->_table[i].present)
        {
          hdata_dpt e = t->_table[i].data;

          spx_latch_lock_s (&e.value->l);
          pgno pg = e.value->pg;
          lsn l = e.value->rec_lsn;
          spx_latch_unlock_s (&e.value->l);

          // Page
          srlizr_write_expect (&s, (u8 *)&pg, sizeof (pg));

          // lsn
          srlizr_write_expect (&s, (u8 *)&l, sizeof (l));
        }
    }

  spx_latch_unlock_s (&t->l);

  return s.dlen;
}

struct dpg_table *
dpgt_deserialize (const u8 *src, u32 dlen, error *e)
{
  struct dpg_table *t = dpgt_open (e);
  if (t == NULL)
    {
      return NULL;
    }

  if (dlen == 0)
    {
      return t;
    }

  struct deserializer d = dsrlizr_create (src, dlen);

  if (d.dlen > MAX_DPGT_SRL_SIZE)
    {
      error_causef (e, ERR_CORRUPT, "DPG table size: %d is invalid", d.dlen);
      return NULL;
    }

  while (true)
    {
      pgno pg;
      lsn l;

      // Page
      bool more = dsrlizr_read ((u8 *)&pg, sizeof (pg), &d);
      if (!more)
        {
          break;
        }

      // lsn
      dsrlizr_read_expect ((u8 *)&l, sizeof (l), &d);

      dpgt_add (t, pg, l, e);
    }

  return t;
}

#ifndef NTEST
void
dpgt_crash (struct dpg_table *t)
{
  DBG_ASSERT (dirty_pg_table, t);
  clck_alloc_close (&t->alloc);
  i_free (t);
}
#endif
