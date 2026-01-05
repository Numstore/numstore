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
 *   Variable hash page for hash-based indexing of variable-length data with collision handling.
 */

#include <numstore/core/hashing.h>
#include <numstore/core/string.h>
#include <numstore/pager/page.h>

////////////////////////////////////////////////////////////
/////// VAR HASH PAGE

/**
 * ============ PAGE START
 * HEADER
 * HASH0     [pgno]
 * HASH1     [pgno]
 * ...
 * HASHn     [pgno]
 * 0         Maybe extra space
 * ============ PAGE END
 */

// OFFSETS and _Static_asserts
#define VH_HASH_OFST PG_COMMN_END
#define VH_HASH_LEN ((PAGE_SIZE - VH_HASH_OFST) / sizeof (pgno))

_Static_assert(PAGE_SIZE > VH_HASH_OFST + 10 * sizeof (pgno),
               "Root Page: PAGE_SIZE must be > RN_HASH_OFST plus at least 10 extra hashes");

// Initialization
void vh_init_empty (page *p);

// Getters
HEADER_FUNC p_size
vh_get_hash_pos (const struct cstring vname)
{
  return (p_size)fnv1a_hash (vname) % (VH_HASH_LEN);
}

HEADER_FUNC pgno
vh_get_hash_value (const page *p, p_size pos)
{
  ASSERT (pos < VH_HASH_LEN);
  PAGE_SIMPLE_GET_IMPL (p, pgno, VH_HASH_OFST + pos * sizeof (pgno));
}

// Setters
HEADER_FUNC void
vh_set_hash_value (page *p, p_size pos, pgno value)
{
  ASSERT (pos < VH_HASH_LEN);
  PAGE_SIMPLE_SET_IMPL (p, value, VH_HASH_OFST + pos * sizeof (pgno));
}

// Validation
err_t vh_validate_for_db (const page *p, error *e);

// Utils
void i_log_vh (int level, const page *vh);
