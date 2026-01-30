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
 *   R+ tree root page structure managing the root of the index tree with overflow handling.
 */

#include <numstore/core/error.h>
#include <numstore/pager/page.h>

/**
 * ============ PAGE START
 * HEADER
 * ROOT     [pgno]
 * NBYTES   [b_size]
 * ============ PAGE END
 */

// OFFSETS and _Static_asserts
#define RR_ROOT_OFST PG_COMMN_END
#define RR_NBYT_OFST ((p_size) (RR_ROOT_OFST + sizeof (pgno)))

// Initialization
void rr_init_empty (page *p);

HEADER_FUNC void
rr_set_root (page *p, pgno pg)
{
  PAGE_SIMPLE_SET_IMPL (p, pg, RR_ROOT_OFST);
}

HEADER_FUNC void
rr_set_nbytes (page *p, b_size s)
{
  PAGE_SIMPLE_SET_IMPL (p, s, RR_NBYT_OFST);
}

// Getters
HEADER_FUNC pgno
rr_get_root (const page *p)
{
  PAGE_SIMPLE_GET_IMPL (p, pgno, RR_ROOT_OFST);
}

HEADER_FUNC pgno
rr_get_nbytes (const page *p)
{
  PAGE_SIMPLE_GET_IMPL (p, b_size, RR_NBYT_OFST);
}

// Validation
err_t rr_validate_for_db (const page *p, error *e);

// Utils
void i_log_rr (int level, const page *rr);
