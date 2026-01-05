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
 *   Command-line tool to print formatted page information from NumStore database files with filtering options.
 */

#include <numstore/usecases.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
parse_params (int argc, char *argv[], pp_params *params)
{
  // Initialize
  params->pgnos = NULL;
  params->pglen = 0;
  params->flag = 0;

  u32 capacity = 0;

  // Skip program name (argv[0])
  for (int i = 2; i < argc; i++)
    {
      char *arg = argv[i];

      // Check for flags
      if (strcmp (arg, "IN") == 0)
        {
          params->flag |= PG_INNER_NODE;
        }
      else if (strcmp (arg, "DL") == 0)
        {
          params->flag |= PG_DATA_LIST;
        }
      else
        {
          char *endptr;
          u64 val = strtoul (arg, &endptr, 10);

          if (*endptr != '\0')
            {
              fprintf (stderr, "Invalid argument: %s\n", arg);
              free (params->pgnos);
              return -1;
            }

          if (params->pglen >= capacity)
            {
              capacity = capacity == 0 ? 4 : capacity * 2;
              u32 *new_pgnos = realloc (params->pgnos, capacity * sizeof (u32));
              if (!new_pgnos)
                {
                  free (params->pgnos);
                  return -1;
                }
              params->pgnos = new_pgnos;
            }

          params->pgnos[params->pglen++] = (u32)val;
        }
    }

  if (params->flag == 0)
    {
      params->flag = PG_ANY;
    }

  return 0;
}

int
main (int argc, char **argv)
{
  pp_params params;
  if (argc == 1 || parse_params (argc, argv, &params))
    {
      printf ("USAGE: nspprint FNAME [PGNO,...] [DL|IN]\n");
      return -1;
    }

  char *fname = argv[1];

  page_print (fname, params);

  return 0;
}
