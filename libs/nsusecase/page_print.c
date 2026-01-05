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
 *   Implements usecases.h. Prints formatted page information from database files
 *   for debugging, with optional filtering by page number and page type.
 */

#include <numstore/core/math.h>
#include <numstore/intf/logging.h>
#include <numstore/pager.h>
#include <numstore/pager/page.h>
#include <numstore/pager/root_node.h>
#include <numstore/usecases.h>

#include <stdlib.h>

static void
_page_print (struct pager *p, pgno pg, pp_params params, error *e)
{
  page_h cur = page_h_create ();

  if (pgr_get (&cur, PG_ANY, pg, p, e))
    {
      error_log_consume (e);
      return;
    }

  if (params.flag & page_h_type (&cur))
    {
      i_log_page (LOG_INFO, page_h_ro (&cur));
    }

  if (pgr_release (p, &cur, PG_ANY, e))
    {
      error_log_consume (e);
      abort ();
    }
}

void
page_print (char *fname, pp_params params)
{
  error e = error_create ();

  struct pager *p = pgr_open (fname, NULL, &e);
  if (p == NULL)
    {
      error_log_consume (&e);
      return;
    }

  for (u32 i = 0; i < pgr_get_npages (p); ++i)
    {
      bool contains;
      arr_contains (params.pgnos, params.pglen, i, contains);
      if (params.pglen == 0 || contains)
        {
          _page_print (p, i, params, &e);
        }
    }

  pgr_close (p, &e);
}
