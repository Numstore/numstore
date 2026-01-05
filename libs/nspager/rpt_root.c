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
 *   Implements rpt_root.h. R+ tree root page operations including tree initialization and root management.
 */

#include <numstore/pager/rpt_root.h>

#include <numstore/core/assert.h>
#include <numstore/core/cbuffer.h>
#include <numstore/core/error.h>
#include <numstore/core/random.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/types.h>
#include <numstore/pager/page.h>
#include <numstore/test/testing.h>

#include "config.h"

// core
// numstore
// system
DEFINE_DBG_ASSERT (
    page, rr_page, v, {
      ASSERT (v);
    })

/////////////////////////////////
///////// INITIALIZATION

void
rr_init_empty (page *p)
{
  ASSERT (page_get_type (p) == PG_RPT_ROOT);
  rr_set_root (p, PGNO_NULL);
  rr_set_nbytes (p, 0);
}

#ifndef NTEST
TEST (TT_UNIT, rr_init_empty)
{
  page p;

  rand_bytes (p.raw, PAGE_SIZE);
  page_init_empty (&p, PG_RPT_ROOT);

  test_assert_equal (rr_get_root (&p), PGNO_NULL);
  test_assert_equal (rr_get_nbytes (&p), 0);
}
#endif

/////////////////////////////////
///////// VALIDATION

err_t
rr_validate_for_db (const page *p, error *e)
{
  if (page_get_type (p) != PG_RPT_ROOT)
    {
      return error_causef (e, ERR_CORRUPT, "Invalid page header for rpt root page");
    }
  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, rr_validate)
{
  page sut;
  error e = error_create ();

  TEST_CASE ("Invalid page type")
  {
    page_init_empty (&sut, PG_DATA_LIST);
    rr_set_root (&sut, 10);
    rr_set_nbytes (&sut, 5);
    test_err_t_check (rr_validate_for_db (&sut, &e), ERR_CORRUPT, &e);
  }
}
#endif

/////////////////////////////////
///////// UTILS

void
i_log_rr (int level, const page *rr)
{
  i_log (level, "=== RPT ROOT PAGE START ===\n");

  i_printf (level, "PGNO:   %" PRpgno "\n", rr->pg);
  i_printf (level, "ROOT:   %" PRpgno "\n", rr_get_root (rr));
  i_printf (level, "NBYTES:   %" PRb_size "\n", rr_get_nbytes (rr));

  i_log (level, "=== RPT ROOT PAGE END ===\n");
}
