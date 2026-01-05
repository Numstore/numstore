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
 *   NumStore client (nstorec) - Command-line REPL client for connecting to NumStore database servers.
 */

// other libraries
#include "core.h"

#include <nsnet/client.h>

// system
int
main (void)
{
  error e = error_create ();

  repl *r = repl_open (
      (repl_params){
          .ip_addr = "127.0.0.1",
          .port = 8123,
      },
      &e);

  if (r == NULL)
    {
      goto theend;
    }

  while (repl_is_running (r))
    {
      if (repl_execute (r, &e))
        {
          goto theend;
        }
    }

theend:
  if (r)
    {
      repl_close (r, &e);
    }
  if (e.cause_code)
    {
      error_log_consume (&e);
      return -1;
    }
  return 0;
}
