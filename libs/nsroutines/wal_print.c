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
 *   Implements usecases.h. Prints write-ahead log (WAL) records from WAL files
 *   for debugging transaction and recovery operations.
 */

#include <numstore/core/error.h>
#include <numstore/intf/logging.h>
#include <numstore/pager/wal_file.h>
#include <numstore/usecases.h>

// numstore
void
walf_print (char *fname)
{
  error e = error_create ();

  struct wal_file wf;
  if (walf_open (&wf, fname, &e))
    {
      error_log_consume (&e);
      return;
    }

  struct wal_rec_hdr_read out;

  while (true)
    {
      lsn rlsn;
      if (walf_read (&out, &rlsn, &wf, &e))
        {
          error_log_consume (&e);
          goto theend;
        }

      i_print_wal_rec_hdr_read_light (LOG_INFO, &out, rlsn);

      if (out.type == WL_EOF)
        {
          break;
        }
    }

theend:
  walf_close (&wf, &e);
}
