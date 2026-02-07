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
 *   Implements var_cursor.h. Provides variable cursor operations for creating,
 *   retrieving, and deleting named variables using a hash-based lookup system
 *   with type serialization.
 */

#include "numstore/core/chunk_alloc.h"
#include <numstore/var/var_cursor.h>

#include <numstore/core/assert.h>
#include <numstore/core/cbuffer.h>
#include <numstore/core/deserializer.h>
#include <numstore/core/error.h>
#include <numstore/core/lalloc.h>
#include <numstore/core/math.h>
#include <numstore/core/random.h>
#include <numstore/core/serializer.h>
#include <numstore/intf/types.h>
#include <numstore/pager.h>
#include <numstore/pager/page.h>
#include <numstore/pager/page_delegate.h>
#include <numstore/pager/page_h.h>
#include <numstore/pager/pager_routines.h>
#include <numstore/pager/root_node.h>
#include <numstore/pager/var_hash_page.h>
#include <numstore/pager/var_page.h>
#include <numstore/test/page_fixture.h>
#include <numstore/test/testing.h>
#include <numstore/types/types.h>

static inline err_t
err_var_doesnt_exist (const struct string vname, error *e)
{
  if (vname.len > 10)
    {
      return error_causef (
          e, ERR_VARIABLE_NE,
          "Variable: %.*s... doesn't exist",
          7, vname.data);
    }
  else
    {
      return error_causef (
          e, ERR_VARIABLE_NE,
          "Variable: %.*s doesn't exist",
          vname.len, vname.data);
    }
}

void
i_log_var_cursor (int log_level, struct var_cursor *r)
{
  i_log (log_level, "================ VAR CURSOR BEGIN ================\n");
  i_printf (log_level, "TX: %" PRtxid "\n", r->tx->tid);
  i_printf (log_level, "CUR: %d %" PRpgno "\n", r->cur.mode, page_h_pgno_or_null (&r->cur));
  i_log (log_level, "================ VAR CURSOR END ================\n");
}

void
varc_enter_transaction (struct var_cursor *r, struct txn *tx)
{
  latch_lock (&r->latch);

  DBG_ASSERT (var_cursor, r);
  ASSERT (r->tx == NULL || r->tx == tx);
  r->tx = tx;

  latch_unlock (&r->latch);
}

void
varc_leave_transaction (struct var_cursor *r)
{
  latch_lock (&r->latch);

  DBG_ASSERT (var_cursor, r);
  ASSERT (r->tx);
  r->tx = NULL;

  latch_unlock (&r->latch);
}

err_t
varh_init_hash_page (struct pager *p, error *e)
{
  page_h hp = page_h_create ();

  // BEGIN TXN
  struct txn tx;
  err_t_wrap (pgr_begin_txn (&tx, p, e), e);

  // See if we already initialized the hash page
  // TODO - make htis error stuff better
  bool print_msg_on_error = e->print_msg_on_error;
  bool print_trace = e->print_trace;
  e->print_msg_on_error = false;
  e->print_trace = false;
  err_t ret = pgr_get (&hp, PG_VAR_HASH_PAGE, VHASH_PGNO, p, e);
  e->print_msg_on_error = print_msg_on_error;
  e->print_trace = print_trace;

  if (ret == ERR_PG_OUT_OF_RANGE)
    {
      error_reset (e);

      // CREATE NEW HASH PAGE
      err_t_wrap (pgr_new (&hp, p, &tx, PG_VAR_HASH_PAGE, e), e);
      ASSERTF (page_h_pgno (&hp) == VHASH_PGNO,
               "Expected page: %" PRpgno " to equal constant hash page: %" PRpgno "\n",
               page_h_pgno (&hp), VHASH_PGNO);
    }
  else if (ret == ERR_CORRUPT)
    {
      error_reset (e);
      err_t_wrap (pgr_get (&hp, PG_ANY, VHASH_PGNO, p, e), e);
      vh_init_empty (page_h_w (&hp));
    }

  err_t_wrap (pgr_release (p, &hp, PG_VAR_HASH_PAGE, e), e);

  // COMMIT
  err_t_wrap (pgr_commit (p, &tx, e), e);

  return SUCCESS;
}

err_t
varc_initialize (struct var_cursor *v, struct pager *p, error *e)
{
  v->pager = p;
  v->cur = page_h_create ();
  v->tx = NULL;
  v->vlen = 0;
  v->tlen = 0;
  latch_init (&v->latch);

  DBG_ASSERT (var_cursor, v);

  return SUCCESS;
}

////////////////////////
/// Functions

static err_t
vpc_read_var_page_here (struct var_cursor *v, error *e)
{
  err_t ret = SUCCESS;

  ASSERT (v->cur.mode != PHM_NONE);
  ASSERT (page_h_type (&v->cur) == PG_VAR_PAGE);

  // Keep the start page so we can reset at the end
  pgno start = page_h_pgno (&v->cur);

  // Start reading this page in
  v->vlen = vp_get_vlen (page_h_ro (&v->cur));
  v->tlen = vp_get_tlen (page_h_ro (&v->cur));

  // Now assert that the values from the page are valid
  ASSERT (v->tlen != 0);
  ASSERT (v->vlen != 0);

  u16 read = 0;
  u16 lread = 0;
  struct cbytes head = dlgt_get_bytes_imut (page_h_ro (&v->cur));

  while (read < v->tlen + v->vlen)
    {
      ASSERT (lread <= head.len);

      // Fetch next node if we ran out of room
      if (lread == head.len)
        {
          page_h next = page_h_create ();
          if ((pgr_dlgt_get_ovnext (&v->cur, &next, NULL, v->pager, e)))
            {
              goto theend;
            }
          if (next.mode == PHM_NONE)
            {
              return error_causef (
                  e, ERR_CORRUPT,
                  "Expected more overhead for variable of name length: "
                  "%d type length: %d. Only read %d bytes so far",
                  v->vlen, v->tlen, read);
            }
          if ((pgr_release (v->pager, &v->cur, PG_VAR_PAGE | PG_VAR_TAIL, e)))
            {
              goto theend;
            }
          v->cur = page_h_xfer_ownership (&next);

          lread = 0;
          head = dlgt_get_bytes_imut (page_h_ro (&v->cur));
        }

      if (read < v->vlen)
        {
          u16 next = MIN (head.len - lread, v->vlen - read);
          if (next > 0)
            {
              i_memcpy (&v->vstr[read], &head.head[lread], next);
              read += next;
              lread += next;
            }
        }

      if (read >= v->vlen)
        {
          u16 next = MIN (head.len - lread, v->vlen + v->tlen - read);
          if (next > 0)
            {
              i_memcpy (&v->tstr[read - v->vlen], &head.head[lread], next);
              read += next;
              lread += next;
            }
        }
    }

  pgno end_should_be_null = dlgt_get_ovnext (page_h_ro (&v->cur));

  // Reset back to head page
  if (page_h_pgno (&v->cur) != start)
    {
      if ((pgr_release (v->pager, &v->cur, PG_VAR_TAIL, e)))
        {
          goto theend;
        }
      if ((pgr_get (&v->cur, PG_VAR_PAGE, start, v->pager, e)))
        {
          goto theend;
        }
    }

  // Check if we are at the end
  if (end_should_be_null != PGNO_NULL)
    {
      error_causef (
          e, ERR_CORRUPT,
          "Done reading variable page, but there's still more overhead. "
          "This is corruption");
      goto theend;
    }

theend:
  return e->cause_code;
}

static inline err_t
vpc_check_and_read_var_page_here (struct var_cursor *v, bool *match, error *e)
{
  err_t ret = SUCCESS;

  ASSERT (v->cur.mode != PHM_NONE);
  ASSERT (page_h_type (&v->cur) == PG_VAR_PAGE);
  ASSERT (match);

  // Keep the start page so we can reset at the end
  pgno start = page_h_pgno (&v->cur);

  // Start reading this page in
  v->vlen = vp_get_vlen (page_h_ro (&v->cur));
  v->tlen = vp_get_tlen (page_h_ro (&v->cur));

  // Now assert that the values from the page are valid
  ASSERT (v->tlen != 0);
  ASSERT (v->vlen != 0);

  // Quick check that lengths aren't compatible
  if (v->vlen != v->vlen_input)
    {
      *match = false;
      goto theend;
    }

  if (vpc_read_var_page_here (v, e))
    {
      goto theend;
    }

  *match = i_memcmp (v->vstr, v->vstr_input, v->vlen) == 0;

theend:
  return e->cause_code;
}

static inline err_t
vpc_write_vstr_tstr_here (struct var_cursor *v, error *e)
{
  err_t ret = SUCCESS;

  ASSERT (v->cur.mode == PHM_X);
  ASSERT (page_h_type (&v->cur) == PG_VAR_PAGE);
  ASSERT (v->tx);

  pgno start = page_h_pgno (&v->cur);
  v->vlen = vp_get_vlen (page_h_ro (&v->cur));
  v->tlen = vp_get_tlen (page_h_ro (&v->cur));

  ASSERT (v->vlen == v->vlen_input);
  ASSERT (v->tlen == v->tlen_input);

  u16 written = 0;
  u16 lwritten = 0;
  struct bytes head = dlgt_get_bytes (page_h_w (&v->cur));

  while (written < v->tlen + v->vlen)
    {
      ASSERT (lwritten <= head.len);

      // Fetch next node if we ran out of room
      if (lwritten == head.len)
        {
          page_h next = page_h_create ();
          if ((pgr_dlgt_new_ovnext_no_link (&v->cur, &next, v->tx, v->pager, e)))
            {
              goto theend;
            }
          if ((pgr_release (v->pager, &v->cur, PG_VAR_PAGE | PG_VAR_TAIL, e)))
            {
              goto theend;
            }
          v->cur = page_h_xfer_ownership (&next);

          lwritten = 0;
          head = dlgt_get_bytes (page_h_w (&v->cur));
        }

      if (written < v->vlen)
        {
          u16 next = MIN (head.len - lwritten, v->vlen - written);
          if (next > 0)
            {
              i_memcpy (&head.head[lwritten], &v->vstr_input[written], next);
              written += next;
              lwritten += next;
            }
        }

      if (written >= v->vlen)
        {
          u16 next = MIN (head.len - lwritten, v->tlen + v->vlen - written);
          if (next > 0)
            {
              i_memcpy (&head.head[lwritten], &v->tstr_input[written - v->vlen], next);
              written += next;
              lwritten += next;
            }
        }
    }

  // Reset back to head page
  if (page_h_pgno (&v->cur) != start)
    {
      if ((pgr_release (v->pager, &v->cur, PG_VAR_TAIL, e)))
        {
          goto theend;
        }
      if ((pgr_get (&v->cur, PG_VAR_PAGE, start, v->pager, e)))
        {
          goto theend;
        }
    }
  else
    {
      if ((pgr_save (v->pager, &v->cur, PG_VAR_PAGE, e)))
        {
          goto theend;
        }
    }

theend:
  v->vlen_input = 0;
  v->tlen_input = 0;
  v->vlen = 0;
  v->tlen = 0;
  return e->cause_code;
}

spgno
vpc_new (struct var_cursor *v, struct var_create_params params, error *e)
{
  DBG_ASSERT (var_cursor, v);
  ASSERT (v->tx);
  ASSERT (type_get_serial_size (&params.t) < sizeof (v->tstr_input));
  ASSERT (params.vname.len < sizeof (v->vstr_input));
  ASSERT (params.vname.len > 0);

  // Move data to input buffers
  {
    struct serializer d = srlizr_create (v->tstr_input, sizeof (v->tstr_input));
    type_serialize (&d, &params.t);
    v->tlen_input = d.dlen;

    v->vlen_input = params.vname.len;
    i_memcpy (v->vstr_input, params.vname.data, v->vlen_input);
  }

  // Fetch the root node
  pgno head;
  p_size pos;
  {
    if ((pgr_get (&v->cur, PG_VAR_HASH_PAGE, VHASH_PGNO, v->pager, e)))
      {
        goto theend;
      }

    // Check hash map
    pos = vh_get_hash_pos (params.vname);
    head = vh_get_hash_value (page_h_ro (&v->cur), pos);

    if ((pgr_release (v->pager, &v->cur, PG_VAR_HASH_PAGE, e)))
      {
        goto theend;
      }
  }

  if (head != PGNO_NULL)
    {
      if ((pgr_get (&v->cur, PG_VAR_PAGE, head, v->pager, e)))
        {
          goto theend;
        }

      // Find the end of the list
      while (true)
        {
          // Check if this var page matches
          {
            bool match;
            if ((vpc_check_and_read_var_page_here (v, &match, e)))
              {
                goto theend;
              }
            if (match)
              {
                error_causef (
                    e, ERR_DUPLICATE_VARIABLE,
                    "Variable: %.*s already exists", params.vname.len, params.vname.data);
                goto theend;
              }
          }

          // Continue
          pgno next = vp_get_next (page_h_ro (&v->cur));

          if (next == PGNO_NULL)
            {
              // Create the next page
              page_h npg = page_h_create ();
              if ((pgr_new (&npg, v->pager, v->tx, PG_VAR_PAGE, e)))
                {
                  goto theend;
                }

              // Update next
              if ((pgr_maybe_make_writable (v->pager, v->tx, &v->cur, e)))
                {
                  goto theend;
                }
              vp_set_next (page_h_w (&v->cur), page_h_pgno (&npg));
              if ((pgr_release (v->pager, &v->cur, PG_VAR_PAGE, e)))
                {
                  goto theend;
                }

              v->cur = page_h_xfer_ownership (&npg);
              break;
            }
          else
            {
              if ((pgr_release (v->pager, &v->cur, PG_VAR_PAGE, e)))
                {
                  goto theend;
                }
              if ((pgr_get (&v->cur, PG_VAR_PAGE, next, v->pager, e)))
                {
                  goto theend;
                }
            }
        }
    }
  else
    {
      if ((pgr_new (&v->cur, v->pager, v->tx, PG_VAR_PAGE, e)))
        {
          goto theend;
        }

      // Set the starting hash page position
      page_h root = page_h_create ();
      if ((pgr_get_writable (&root, v->tx, PG_VAR_HASH_PAGE, VHASH_PGNO, v->pager, e)))
        {
          goto theend;
        }
      vh_set_hash_value (page_h_w (&root), pos, page_h_pgno (&v->cur));
      if ((pgr_release (v->pager, &root, PG_VAR_HASH_PAGE, e)))
        {
          goto theend;
        }
    }

  vp_set_next (page_h_w (&v->cur), PGNO_NULL);
  vp_set_ovnext (page_h_w (&v->cur), PGNO_NULL);
  vp_set_vlen (page_h_w (&v->cur), v->vlen_input);
  vp_set_tlen (page_h_w (&v->cur), v->tlen_input);
  vp_set_root (page_h_w (&v->cur), PGNO_NULL);
  vp_set_nbytes (page_h_w (&v->cur), 0);

  if ((vpc_write_vstr_tstr_here (v, e)))
    {
      goto theend;
    }

  pgno ret = page_h_pgno (&v->cur);

  if ((pgr_release (v->pager, &v->cur, PG_VAR_PAGE, e)))
    {
      goto theend;
    }

theend:

  v->vlen_input = 0;
  v->tlen_input = 0;
  v->vlen = 0;
  v->tlen = 0;

  if (e->cause_code)
    {
      return e->cause_code;
    }
  else
    {
      return ret;
    }
}

spgno
vpc_get (struct var_cursor *v, struct chunk_alloc *dalloc, struct var_get_params *dest, error *e)
{
  DBG_ASSERT (var_cursor, v);
  ASSERT (dest);
  ASSERT (dest->vname.len > 0);
  ASSERT (dest->vname.data);

  spgno ret = 0;

  // Init query params
  // Move data to input buffers
  {
    v->vlen_input = dest->vname.len;
    i_memcpy (v->vstr_input, dest->vname.data, v->vlen_input);
  }

  // Check hash map
  {
    if ((pgr_get (&v->cur, PG_VAR_HASH_PAGE, VHASH_PGNO, v->pager, e)))
      {
        goto theend;
      }

    p_size pos = vh_get_hash_pos (dest->vname);
    pgno head = vh_get_hash_value (page_h_ro (&v->cur), pos);

    if ((pgr_release (v->pager, &v->cur, PG_VAR_HASH_PAGE, e)))
      {
        goto theend;
      }

    if (head == PGNO_NULL)
      {
        err_var_doesnt_exist (dest->vname, e);

        goto theend;
      }

    if ((pgr_get (&v->cur, PG_VAR_PAGE, head, v->pager, e)))
      {
        goto theend;
      }
  }

  // Find the entry in the chain
  while (true)
    {
      bool match;
      if ((vpc_check_and_read_var_page_here (v, &match, e)))
        {
          goto theend;
        }

      if (match)
        {
          // Type
          if (dalloc)
            {
              dest->t = chunk_malloc (dalloc, 1, sizeof (struct type), e);
              if (dest->t == NULL)
                {
                  goto theend;
                }
              struct deserializer d = dsrlizr_create (v->tstr, v->tlen);
              if ((type_deserialize (dest->t, &d, dalloc, e)))
                {
                  goto theend;
                }
              // You already have the name so no need to allocate
            }

          // Root
          dest->rpt_root = vp_get_root (page_h_ro (&v->cur));
          dest->var_root = page_h_pgno (&v->cur);
          dest->nbytes = vp_get_nbytes (page_h_ro (&v->cur));
          ret = page_h_pgno (&v->cur);

          pgr_release (v->pager, &v->cur, PG_VAR_PAGE, e);

          goto theend;
        }

      // No match - advance to next node
      pgno next = vp_get_next (page_h_ro (&v->cur));
      if ((pgr_release (v->pager, &v->cur, PG_VAR_PAGE, e)))
        {
          goto theend;
        }

      if (next == PGNO_NULL)
        {
          err_var_doesnt_exist (dest->vname, e);
          goto theend;
        }
      else
        {
          if (pgr_get (&v->cur, PG_VAR_PAGE, next, v->pager, e))
            {
              goto theend;
            }
        }
    }

theend:
  v->vlen_input = 0;
  v->tlen_input = 0;
  v->vlen = 0;
  v->tlen = 0;
  if (e->cause_code)
    {
      return e->cause_code;
    }
  else
    {
      return ret;
    }
}

err_t
vpc_update_by_id (
    struct var_cursor *v,
    struct var_update_by_id_params *src,
    error *e)
{
  DBG_ASSERT (var_cursor, v);
  ASSERT (src);
  ASSERT (v->tx);

  if ((pgr_get_writable (&v->cur, v->tx, PG_VAR_PAGE, src->id, v->pager, e)))
    {
      goto theend;
    }

  vp_set_root (page_h_w (&v->cur), src->root);
  vp_set_nbytes (page_h_w (&v->cur), src->nbytes);

  if (pgr_release (v->pager, &v->cur, PG_VAR_PAGE, e))
    {
      goto theend;
    }

theend:
  return e->cause_code;
}

err_t
vpc_get_by_id (
    struct var_cursor *v,
    struct chunk_alloc *dalloc,
    struct var_get_by_id_params *dest,
    error *e)
{
  DBG_ASSERT (var_cursor, v);
  ASSERT (dest);

  if ((pgr_get (&v->cur, PG_VAR_PAGE, dest->id, v->pager, e)))
    {
      goto theend;
    }

  // Deserialize the Type and name
  if (dalloc)
    {
      // All the other info fits on this page, only need to read here if we need type / name
      if ((vpc_read_var_page_here (v, e)))
        {
          goto theend;
        }

      struct deserializer d = dsrlizr_create (v->tstr, v->tlen);
      if ((type_deserialize (&dest->t, &d, dalloc, e)))
        {
          goto theend;
        }
      dest->name = chunk_alloc_move_mem (dalloc, v->vstr, v->vlen, e);
      if (dest->name == NULL)
        {
          goto theend;
        }
      dest->nlen = v->vlen;
    }

  dest->pg0 = vp_get_root (page_h_ro (&v->cur));
  dest->nbytes = vp_get_nbytes (page_h_ro (&v->cur));

  if (pgr_release (v->pager, &v->cur, PG_VAR_PAGE, e))
    {
      goto theend;
    }

theend:
  v->vlen_input = 0;
  v->tlen_input = 0;
  v->vlen = 0;
  v->tlen = 0;
  return e->cause_code;
}

err_t
vpc_delete (struct var_cursor *v, const struct string name, error *e)
{
  DBG_ASSERT (var_cursor, v);
  ASSERT (v->tx);
  ASSERT (name.len < MAX_VSTR);

  // Move data to input buffers
  v->vlen_input = name.len;
  i_memcpy (v->vstr_input, name.data, v->vlen_input);

  // Fetch the root node
  if ((pgr_get (&v->cur, PG_VAR_HASH_PAGE, VHASH_PGNO, v->pager, e)))
    {
      goto theend;
    }

  // Check hash map
  p_size pos = vh_get_hash_pos (name);
  pgno head = vh_get_hash_value (page_h_ro (&v->cur), pos);

  if ((pgr_release (v->pager, &v->cur, PG_VAR_HASH_PAGE, e)))
    {
      goto theend;
    }

  page_h prev = page_h_create ();

  if (head != PGNO_NULL)
    {
      if ((pgr_get (&v->cur, PG_VAR_PAGE, head, v->pager, e)))
        {
          goto theend;
        }

      // Find the matching node
      while (true)
        {
          bool match;
          if ((vpc_check_and_read_var_page_here (v, &match, e)))
            {
              goto theend;
            }
          if (match)
            {
              if (prev.mode == PHM_NONE)
                {
                  // UPDATE ROOT
                  page_h root = page_h_create ();
                  if ((pgr_get_writable (&root, v->tx, PG_VAR_HASH_PAGE, VHASH_PGNO, v->pager, e)))
                    {
                      goto theend;
                    }
                  vh_set_hash_value (page_h_w (&root), pos, vp_get_next (page_h_ro (&v->cur)));
                  if ((pgr_release (v->pager, &root, PG_VAR_HASH_PAGE, e)))
                    {
                      goto theend;
                    }
                }
              else
                {
                  // prev->next = cur->next
                  if ((pgr_make_writable (v->pager, v->tx, &prev, e)))
                    {
                      goto theend;
                    }
                  vp_set_next (page_h_w (&prev), vp_get_next (page_h_ro (&v->cur)));
                  if ((pgr_release (v->pager, &prev, PG_VAR_PAGE, e)))
                    {
                      goto theend;
                    }
                }

              if ((pgr_dlgt_delete_vpvl_chain (&v->cur, v->tx, v->pager, e)))
                {
                  goto theend;
                }
              goto theend;
            }

          if (prev.mode != PHM_NONE)
            {
              if ((pgr_release (v->pager, &prev, PG_VAR_PAGE, e)))
                {
                  goto theend;
                }
            }
          prev = page_h_xfer_ownership (&v->cur);

          pgno next = vp_get_next (page_h_ro (&prev));

          if (next == PGNO_NULL)
            {
              if ((pgr_release (v->pager, &prev, PG_VAR_PAGE, e)))
                {
                  goto theend;
                }
              err_var_doesnt_exist (name, e);
              goto theend;
            }
          else
            {
              if ((pgr_get (&v->cur, PG_VAR_PAGE, next, v->pager, e)))
                {
                  goto theend;
                }
            }
        }
    }
  else
    {
      if ((pgr_release (v->pager, &v->cur, PG_VAR_PAGE, e)))
        {
          goto theend;
        }
      err_var_doesnt_exist (name, e);
      goto theend;
    }

theend:
  v->vlen_input = 0;
  v->tlen_input = 0;
  v->vlen = 0;
  v->tlen = 0;
  return e->cause_code;
}

#ifndef NTEST
RANDOM_TEST (TT_UNIT, vpc_write_and_verify, 1)
{
  struct pgr_fixture f;
  struct var_cursor v;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  test_err_t_wrap (varh_init_hash_page (f.p, &f.e), &f.e);
  test_err_t_wrap (varc_initialize (&v, f.p, &f.e), &f.e);

  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  varc_enter_transaction (&v, &tx);

  TEST_AGENT (1, "Create then read")
  {
    // Create a random string
    u32 len = randu32r (1, MAX_VSTR);
    u8 *_src = i_malloc (len, 1, &f.e);
    test_assert (_src != NULL);
    rand_bytes (_src, len);

    // Create a new variable
    {
      struct var_create_params src = {
        .vname = (struct string){
            .len = len,
            .data = (char *)_src,
        },
        .t = (struct type){
            .type = T_PRIM,
            .p = U32,
        },
      };

      test_err_t_wrap (vpc_new (&v, src, &f.e), &f.e);
    }

    // Fetch the variable you just created
    struct var_get_params dest;
    {
      dest.vname = (struct string){
        .len = len,
        .data = (char *)_src,
      };

      struct chunk_alloc alloc;
      chunk_alloc_create_default (&alloc);
      test_err_t_wrap (vpc_get (&v, &alloc, &dest, &f.e), &f.e);
    }

    // Validate data
    {
      test_assert_int_equal (dest.t->type, T_PRIM);
      test_assert_int_equal (dest.t->p, U32);
    }

    i_free (_src);
  }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);
  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

RANDOM_TEST (TT_UNIT, vpc_write_and_delete, 1)
{
  struct pgr_fixture f;
  struct var_cursor v;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  test_err_t_wrap (varh_init_hash_page (f.p, &f.e), &f.e);
  f.e.print_trace = true;
  test_err_t_wrap (varc_initialize (&v, f.p, &f.e), &f.e);

  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  varc_enter_transaction (&v, &tx);

  TEST_AGENT (1, "Create then delete then get")
  {
    u32 len = randu32r (1, MAX_VSTR);
    u8 *_src = i_malloc (len, 1, &f.e);
    test_assert (_src != NULL);
    rand_bytes (_src, len);

    // Create the variable
    {
      struct var_create_params src = {
        .vname = (struct string){
            .len = len,
            .data = (char *)_src,
        },
        .t = (struct type){
            .type = T_PRIM,
            .p = U32,
        },
      };

      test_err_t_wrap (vpc_new (&v, src, &f.e), &f.e);
    }

    // Delete the variable
    test_err_t_wrap (vpc_delete (
                         &v, (struct string){
                                 .data = (char *)_src,
                                 .len = len,
                             },
                         &f.e),
                     &f.e);

    // Get the variable
    {
      struct var_get_params dest;
      dest.vname = (struct string){
        .len = len,
        .data = (char *)_src,
      };

      struct chunk_alloc alloc;
      chunk_alloc_create_default (&alloc);
      test_err_t_check (vpc_get (&v, &alloc, &dest, &f.e), ERR_VARIABLE_NE, &f.e);
    }

    i_free (_src);
  }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);
  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}

RANDOM_TEST (TT_HEAVY, vpc_write_and_verify_then_write_and_delete, 1)
{
  struct pgr_fixture f;
  struct var_cursor v;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);
  f.e.print_trace = true;
  test_err_t_wrap (varc_initialize (&v, f.p, &f.e), &f.e);

  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  varc_enter_transaction (&v, &tx);

  TEST_AGENT (1, "Create then ")
  {
    u32 len = randu32r (1, MAX_VSTR);
    u8 *_src = i_malloc (len, 1, &f.e);
    test_assert (_src != NULL);
    rand_bytes (_src, len);

    // Create the variable
    {
      struct var_create_params src = {
        .vname = (struct string){
            .len = len,
            .data = (char *)_src,
        },
        .t = (struct type){
            .type = T_PRIM,
            .p = U32,
        },
      };

      test_err_t_wrap (vpc_new (&v, src, &f.e), &f.e);
    }

    // Get the variable
    struct var_get_params dest;
    {
      dest.vname = (struct string){
        .len = len,
        .data = (char *)_src,
      };

      struct chunk_alloc alloc;
      chunk_alloc_create_default (&alloc);
      test_err_t_wrap (vpc_get (&v, &alloc, &dest, &f.e), &f.e);
    }

    // Verify
    {
      test_assert_int_equal (dest.t->type, T_PRIM);
      test_assert_int_equal (dest.t->p, U32);
    }

    i_free (_src);
  }

  TEST_AGENT (1, "Create then delete then get")
  {
    u32 len = randu32r (1, MAX_VSTR);
    u8 *_src = i_malloc (len, 1, &f.e);
    test_assert (_src != NULL);
    rand_bytes (_src, len);

    // Create the variable
    {
      struct var_create_params src = {
        .vname = (struct string){
            .len = len,
            .data = (char *)_src,
        },
        .t = (struct type){
            .type = T_PRIM,
            .p = U32,
        },
      };

      test_err_t_wrap (vpc_new (&v, src, &f.e), &f.e);
    }

    // Delete the variable
    test_err_t_wrap (vpc_delete (
                         &v, (struct string){
                                 .data = (char *)_src,
                                 .len = len,
                             },
                         &f.e),
                     &f.e);

    // Get the variable
    {
      struct var_get_params dest;
      dest.vname = (struct string){
        .len = len,
        .data = (char *)_src,
      };

      struct chunk_alloc alloc;
      chunk_alloc_create_default (&alloc);
      test_err_t_check (vpc_get (&v, &alloc, &dest, &f.e), ERR_VARIABLE_NE, &f.e);
    }

    i_free (_src);
  }

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);
  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}
#endif
