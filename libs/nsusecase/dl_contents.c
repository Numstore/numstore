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
 *   Implements usecases.h. Reads and outputs data list page contents for debugging,
 *   traversing the linked list of data pages and writing their raw data to a stream.
 */

#include <numstore/pager.h>
#include <numstore/pager/pager_routines.h>
#include <numstore/usecases.h>

#include <stdlib.h>

static void
dl_contents_one_page (FILE *out, struct pager *p, page_h *cur, error *e)
{
  fprintf (stderr, "============================= %" PRpgno "\n", page_h_pgno (cur));
  fprintf (stderr, "DATA_LIST\n");
  fprintf (stderr, "next: %lu\n", dl_get_next (page_h_ro (cur)));
  fprintf (stderr, "prev: %lu\n", dl_get_prev (page_h_ro (cur)));
  fprintf (stderr, "blen: %d\n", dl_used (page_h_ro (cur)));

  u8 output[DL_DATA_SIZE];
  p_size read = dl_read (page_h_ro (cur), output, 0, DL_DATA_SIZE);

  size_t total = 0;
  while (total < read)
    {
      size_t written = fwrite (output + total, 1, read - total, out);
      if (written == 0)
        {
          if (ferror (out))
            {
              perror ("fwrite");
              break;
            }
          break;
        }
      total += written;
    }

  fprintf (stderr, "Wrote: %lu\n", total);
}

void
dl_contents (FILE *out, char *fname, pgno pg)
{
  error e = error_create ();

  struct pager *p = pgr_open (fname, "test.wal", &e);
  if (p == NULL)
    {
      error_log_consume (&e);
      return;
    }

  page_h cur = page_h_create ();
  if (pgr_get (&cur, PG_DATA_LIST | PG_INNER_NODE, pg, p, &e))
    {
      error_log_consume (&e);
      abort ();
    }

  while (true)
    {
      if (cur.mode == PHM_NONE)
        {
          pgr_close (p, &e);
          return;
        }
      dl_contents_one_page (out, p, &cur, &e);
      if (pgr_dlgt_advance_next (&cur, NULL, p, &e))
        {
          error_log_consume (&e);
          return;
        }
    }
}
