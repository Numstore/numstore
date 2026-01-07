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
 *   Scratch/example program demonstrating write operations.
 */

// core
#include <numstore/core/cbuffer.h>
#include <numstore/core/error.h>
#include <numstore/core/math.h>
#include <numstore/nslib/rptree/rptree.h>
#include <numstore/pager.h>

// numstore
int
main (void)
{
  error e = error_create ();

  struct lockt lt;
  test_err_t_wrap (lockt_init (&lt, &e), &e);

  struct thread_pool *tp = tp_open (&e);
  test_fail_if_null (tp);
  int src[20480];

  arr_range (src);

  struct pager *p = pgr_open ("test.db", "test.wal", &lt, tp, &e);
  if (p == NULL)
    {
      return e.cause_code;
    }

  struct rptree *r = rpt_new (p, &e);
  if (r == NULL)
    {
      return e.cause_code;
    }

  stxid tid = pgr_begin_txn (p, &e);
  if (tid < 0)
    {
      return e.cause_code;
    }

  err_t ret = rpt_insert (
      r, (struct rpti){
             .src = src,
             .t = tid,
             .bsize = 4,
             .nelems = arrlen (src),
             .ofst = 0,
         },
      &e);

  if (ret < 0)
    {
      return e.cause_code;
    }

  if (rpt_close (r, &e))
    {
      return e.cause_code;
    }
  if (pgr_close (p, &e))
    {
      return e.cause_code;
    }

  return 0;
}
