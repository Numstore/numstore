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
 *   Implements tombstone.h. Tombstone page operations for deletion tracking and cleanup.
 */

#include <numstore/pager/tombstone.h>

#include <numstore/core/random.h>
#include <numstore/intf/logging.h>
#include <numstore/pager/page.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    page, tmbst_page, d,
    {
      ASSERT (d);
    })

/////////////////////////////////
///////// INITIALIZATION

#ifndef NTEST
TEST (TT_UNIT, tmbst_init_empty)
{
  page p;

  rand_bytes (p.raw, PAGE_SIZE);
  page_init_empty (&p, PG_TOMBSTONE);

  test_assert_int_equal (page_get_type (&p), PG_TOMBSTONE);
  test_assert_equal (tmbst_get_next (&p), PGNO_NULL);
}
#endif

err_t
tmbst_validate_for_db (const page *p, error *e)
{
  if (page_get_type (p) != PG_TOMBSTONE)
    {
      return error_causef (e, ERR_CORRUPT, "Invalid page header for tombstone");
    }
  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, tmbst_validate_for_db)
{
  error e = error_create ();
  page p;

  TEST_CASE ("Invalid header -> ERR_CORRUPT")
  {
    rand_bytes (p.raw, PAGE_SIZE);
    page_set_type (&p, PG_DATA_LIST);
    test_assert_int_equal (tmbst_validate_for_db (&p, &e), ERR_CORRUPT);
    e.cause_code = SUCCESS;
  }

  TEST_CASE ("Valid header -> SUCCESS")
  {
    rand_bytes (p.raw, PAGE_SIZE);
    page_init_empty (&p, PG_TOMBSTONE);
    test_assert_int_equal (tmbst_validate_for_db (&p, &e), SUCCESS);
  }

  TEST_CASE ("Invalid header -> ERR_CORRUPT")
  {
    rand_bytes (p.raw, PAGE_SIZE);
    page_set_type (&p, PG_ROOT_NODE);
    test_assert_int_equal (tmbst_validate_for_db (&p, &e), ERR_CORRUPT);
    e.cause_code = SUCCESS;
  }

  TEST_CASE ("Valid header -> SUCCESS")
  {
    rand_bytes (p.raw, PAGE_SIZE);
    page_init_empty (&p, PG_TOMBSTONE);
    test_assert_int_equal (tmbst_validate_for_db (&p, &e), SUCCESS);
  }
}
#endif

/////////////////////////////////
///////// UTILS

void
i_log_tmbst (int level, const page *t)
{
  i_log (level, "=== TMBST PAGE START ===\n");

  i_printf (level, "PGNO: %" PRpgno "\n", t->pg);

  if (tmbst_get_next (t) == PGNO_NULL)
    {
      i_printf (level, "NEXT: NULL\n");
    }
  else
    {
      i_printf (level, "NEXT: %" PRpgno "\n", tmbst_get_next (t));
    }

  i_log (level, "=== TMBST PAGE END ===\n");
}
