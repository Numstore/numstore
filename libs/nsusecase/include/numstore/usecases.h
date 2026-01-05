#pragma once

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
 *   Utility functions for debugging and inspecting NumStore internal data structures.
 *   Provides interfaces for printing data list contents, database pages, and WAL records.
 */

#include <numstore/intf/types.h>
#include <numstore/pager/page.h>

#include <stdio.h>

void dl_contents (FILE *out, char *fname, pgno pg);

typedef struct
{
  u32 *pgnos;
  u32 pglen;
  u32 pgcap;
  int flag;
} pp_params;

void page_print (char *fname, pp_params pg);
void walf_print (char *fname);
