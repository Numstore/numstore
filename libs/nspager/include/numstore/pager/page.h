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
 *   Core page structure and operations for all page types including lifecycle management, type checking, and page handle operations.
 */

#include <numstore/core/assert.h>
#include <numstore/core/bytes.h>
#include <numstore/core/error.h>
#include <numstore/intf/stdlib.h>

#include <config.h>

enum page_type
{
  // Common page types
  PG_TOMBSTONE = (1 << 0), // An empty node - availble to be used
  PG_ROOT_NODE = (1 << 1), // The first page with db level meta data

  // Rptree page types
  PG_DATA_LIST = (1 << 2),  // r+tree data node
  PG_INNER_NODE = (1 << 3), // r+tree Inner node

  // Variable page types
  PG_VAR_HASH_PAGE = (1 << 4), // A Hash Table for variable names - links to a linked list
  PG_VAR_PAGE = (1 << 5),      // A Single link in the hash table linked list
  PG_VAR_TAIL = (1 << 6),      // Overflow to a VAR_PAGE
};

#define PG_ANY (PG_TOMBSTONE       \
                | PG_ROOT_NODE     \
                | PG_DATA_LIST     \
                | PG_INNER_NODE    \
                | PG_VAR_HASH_PAGE \
                | PG_VAR_PAGE      \
                | PG_VAR_TAIL)

// COMMON PAGE HEADER
#define PG_CKSM_OFST ((p_size)0)
#define PG_HEDR_OFST ((p_size)(PG_CKSM_OFST + sizeof (u32)))
#define PG_PLSN_OFST ((p_size)(PG_HEDR_OFST + sizeof (pgh)))
#define PG_COMMN_END ((p_size)(PG_PLSN_OFST + sizeof (lsn)))

typedef struct
{
  u8 raw[PAGE_SIZE];
  pgno pg;
} page;

DEFINE_DBG_ASSERT (
    page, page_base, p,
    {
      ASSERT (p);
    })

// Initialization
void page_init_empty (page *p, enum page_type t);

// Validate
err_t page_validate_for_db (const page *p, int page_types, error *e);

////////////////////////////////////////////////////////////
/////// Utility Macros

#define PAGE_SIMPLE_GET_IMPL(v, type, ofst)             \
  do                                                    \
    {                                                   \
      ASSERT ((ofst) + sizeof (type) < PAGE_SIZE);      \
      type ret;                                         \
      i_memcpy (&(ret), &(v)->raw[ofst], sizeof (ret)); \
      return ret;                                       \
    }                                                   \
  while (0)

#define PAGE_SIMPLE_SET_IMPL(v, val, ofst)              \
  do                                                    \
    {                                                   \
      ASSERT ((ofst) + sizeof (val) < PAGE_SIZE);       \
      i_memcpy (&(v)->raw[ofst], &(val), sizeof (val)); \
    }                                                   \
  while (0)

////////////////////////////////////////////////////////////
///////// GETTERS

HEADER_FUNC u32
page_get_checksum (const page *p)
{
  DBG_ASSERT (page_base, p);
  PAGE_SIMPLE_GET_IMPL (p, u32, PG_CKSM_OFST);
}

HEADER_FUNC pgh
page_get_type (const page *p)
{
  PAGE_SIMPLE_GET_IMPL (p, pgh, PG_HEDR_OFST);
}

HEADER_FUNC lsn
page_get_page_lsn (const page *p)
{
  DBG_ASSERT (page_base, p);
  PAGE_SIMPLE_GET_IMPL (p, lsn, PG_PLSN_OFST);
}

////////////////////////////////////////////////////////////
///////// SETTERS

HEADER_FUNC void
page_set_checksum (page *p, u32 checksum)
{
  DBG_ASSERT (page_base, p);
  PAGE_SIMPLE_SET_IMPL (p, checksum, PG_CKSM_OFST);
}

HEADER_FUNC void
page_set_type (page *p, enum page_type t)
{
  DBG_ASSERT (page_base, p);
  pgh _type = t;
  PAGE_SIMPLE_SET_IMPL (p, _type, PG_HEDR_OFST);
}

HEADER_FUNC void
page_set_page_lsn (page *p, lsn page_lsn)
{
  DBG_ASSERT (page_base, p);
  PAGE_SIMPLE_SET_IMPL (p, page_lsn, PG_PLSN_OFST);
}

HEADER_FUNC void
page_memcpy (page *dest, struct bytes src)
{
  DBG_ASSERT (page_base, dest);
  ASSERT (src.len == PAGE_SIZE);
  i_memcpy (dest->raw, src.head, src.len);
}

// Logging
void i_log_page (int log_level, const page *p);
