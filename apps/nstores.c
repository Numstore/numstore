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
 *   NumStore server (nstores) - Database server daemon that listens for client connections and processes queries.
 */

// other libraries
#include "core.h"

#include <nsnet/server.h>

// system
int
main (void)
{
  error e = error_create ();

  if (i_remove_quiet (strfcstr ("test.db"), &e))
    {
      error_log_consume (&e);
      return -1;
    }

  server *s = server_open (8123, strfcstr ("test.db"), &e);

  if (s == NULL)
    {
      goto theend;
    }

  while (!server_is_done (s))
    {
      server_execute (s);
    }

theend:
  if (s)
    {
      server_close (s, &e);
    }
  if (e.cause_code)
    {
      error_log_consume (&e);
      return -1;
    }
  return 0;
}
