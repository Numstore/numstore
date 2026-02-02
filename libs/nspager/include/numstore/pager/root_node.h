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
 *   Root node page structure holding metadata about the database including file structure and top-level pointers.
 */

#include <numstore/core/string.h>
#include <numstore/pager/page.h>

/**
 * ============ PAGE START
 * HEADER
 * FSTS     [pgno]  - First tombstone
 * MLSN     [lsn]   - Master lsn
 * ============ PAGE END
 */

// OFFSETS and _Static_asserts
#define RN_FSTS_OFST PG_COMMN_END                              // First tombstone
#define RN_MLSN_OFST ((p_size) (RN_FSTS_OFST + sizeof (pgno))) // Master LSN

// Initialization

// Setters
HEADER_FUNC void
rn_set_first_tmbst (page *p, pgno pg)
{
  PAGE_SIMPLE_SET_IMPL (p, pg, RN_FSTS_OFST);
}

HEADER_FUNC void
rn_set_master_lsn (page *p, lsn pg)
{
  PAGE_SIMPLE_SET_IMPL (p, pg, RN_MLSN_OFST);
}

HEADER_FUNC void
rn_init_empty (page *rn)
{
  ASSERT (page_get_type (rn) == PG_ROOT_NODE);
  rn_set_first_tmbst (rn, 1);
  rn_set_master_lsn (rn, 0);
}

// Getters
HEADER_FUNC pgno
rn_get_first_tmbst (const page *p)
{
  PAGE_SIMPLE_GET_IMPL (p, pgno, RN_FSTS_OFST);
}

HEADER_FUNC lsn
rn_get_master_lsn (const page *p)
{
  PAGE_SIMPLE_GET_IMPL (p, pgno, RN_MLSN_OFST);
}

// Validation
err_t rn_validate_for_db (const page *p, error *e);

// Utils
void i_log_rn (int level, const page *rn);
