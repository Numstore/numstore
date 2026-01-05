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
 *   Data list page structure for storing variable-length data in linked list pages with forward/backward pointers and variable-sized data sections.
 */

#include <numstore/core/cbuffer.h>
#include <numstore/core/error.h>
#include <numstore/pager/page.h>

DEFINE_DBG_ASSERT (
    page, data_list, d, {
      ASSERT (d);
    })

// OFFSETS and _Static_asserts
#define DL_NEXT_OFST PG_COMMN_END
#define DL_PREV_OFST ((p_size) (DL_NEXT_OFST + sizeof (pgno)))
#define DL_BLEN_OFST ((p_size) (DL_PREV_OFST + sizeof (pgno)))
#define DL_DATA_OFST ((p_size) (DL_BLEN_OFST + sizeof (p_size)))

_Static_assert(
    PAGE_SIZE > DL_DATA_OFST + 10,
    "Data List: PAGE_SIZE must be > DL_DATA_OFST "
    "plus at least 10 extra bytes of data");

#define DL_DATA_SIZE ((p_size) (PAGE_SIZE - DL_DATA_OFST))
#define DL_REM (DL_DATA_SIZE % 2)

struct dl_data
{
  void *data;
  u32 blen;
};

err_t dl_validate_for_db (const page *d, error *e);
p_size dl_append (page *d, const u8 *src, p_size bytes);
void dl_append_from_cbuffer (page *d, struct cbuffer *src, p_size amnt);
p_size dl_write (page *d, const u8 *src, p_size offset, p_size bytes);
p_size dl_write_from_buffer (page *d, struct cbuffer *src, p_size offset, p_size nbytes);
p_size dl_memset_from_buffer (page *d, struct cbuffer *src, p_size nbytes);
void dl_memset_from_buffer_expect (page *d, struct cbuffer *src, p_size nbytes);
void dl_memset (page *d, u8 *buf, p_size len);
void dl_set_data (page *p, struct dl_data d);
void dl_move_left (page *dest, page *src, p_size len);
void dl_move_right (page *src, page *dest, p_size len);
void i_log_dl (int level, const page *d);
p_size dl_read (const page *d, u8 *dest, p_size offset, p_size bytes);
p_size dl_read_into_cbuffer (const page *d, struct cbuffer *c, p_size offset, p_size bytes);
p_size dl_read_out_into_cbuffer (page *d, struct cbuffer *dest, p_size offset, p_size bytes);
void dl_read_expect (const page *d, u8 *dest, p_size offset, p_size bytes);
p_size dl_read_out_from (page *d, u8 *dest, p_size offset);
void dl_shift_right (page *d, p_size len);
void dl_make_valid (page *d);

////////////////////////////////////////////////////////////
//////// GETTERS

HEADER_FUNC pgno
dl_get_next (const page *d)
{
  PAGE_SIMPLE_GET_IMPL (d, pgno, DL_NEXT_OFST);
}

HEADER_FUNC pgno
dl_get_prev (const page *d)
{
  PAGE_SIMPLE_GET_IMPL (d, pgno, DL_PREV_OFST);
}

HEADER_FUNC void *
dl_get_data (const page *d)
{
  return (void *)&d->raw[DL_DATA_OFST];
}

HEADER_FUNC p_size
dl_used (const page *d)
{
  PAGE_SIMPLE_GET_IMPL (d, p_size, DL_BLEN_OFST);
}

HEADER_FUNC u8
dl_get_byte (const page *d, p_size i)
{
  ASSERT (i < dl_used (d));
  return ((u8 *)dl_get_data (d))[i];
}

HEADER_FUNC p_size
dl_avail (const page *d)
{
  return DL_DATA_SIZE - dl_used (d);
}

HEADER_FUNC bool
dl_is_root (const page *p)
{
  DBG_ASSERT (data_list, p);
  return dl_get_next (p) == PGNO_NULL && dl_get_prev (p) == PGNO_NULL;
}

// Shorthands
#define dl_full(dl) (dl_used (dl) == DL_DATA_SIZE)

////////////////////////////////////////////////////////////
//////// SETTERS

HEADER_FUNC void
dl_set_next (page *d, pgno next)
{
  PAGE_SIMPLE_SET_IMPL (d, next, DL_NEXT_OFST);
}

HEADER_FUNC void
dl_set_prev (page *d, pgno prev)
{
  PAGE_SIMPLE_SET_IMPL (d, prev, DL_PREV_OFST);
}

HEADER_FUNC void
dl_set_byte (page *d, p_size i, u8 byte)
{
  ASSERT (i < dl_used (d));
  ((u8 *)dl_get_data (d))[i] = byte;
}

HEADER_FUNC void
dl_set_used (page *d, p_size used)
{
  ASSERT (used <= DL_DATA_SIZE);
  PAGE_SIMPLE_SET_IMPL (d, used, DL_BLEN_OFST);
}

HEADER_FUNC void
dl_init_empty (page *d)
{
  ASSERT (page_get_type (d) == PG_DATA_LIST);
  dl_set_next (d, PGNO_NULL);
  dl_set_prev (d, PGNO_NULL);
  dl_set_used (d, 0);
}

HEADER_FUNC void
dl_dl_memmove_permissive (page *dest, const page *src, p_size didx, p_size sidx, p_size nbytes)
{
  ASSERT (dest);
  ASSERT (src);

  ASSERT (sidx < DL_DATA_SIZE);
  ASSERT (sidx + nbytes <= DL_DATA_SIZE);

  if (dest->pg == src->pg)
    {
      ASSERT (sidx > didx); // Nothing to do on same ptr
      if (didx == sidx)
        {
          return;
        }
    }

  i_memmove ((u8 *)dl_get_data (dest) + didx, (u8 *)dl_get_data (src) + sidx, nbytes);
}
