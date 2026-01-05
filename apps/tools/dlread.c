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
 *   Command-line tool to read and output data list page contents from NumStore database files.
 */

// system
#include <numstore/usecases.h>

#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char **argv)
{
  if (argc != 3)
    {
      printf ("USAGE: dlread FNAME PGNO\n");
      return -1;
    }

  int pg;

  pg = atoi (argv[2]);
  char *fname = argv[1];

  dl_contents (stdout, fname, pg);

  return 0;
}
