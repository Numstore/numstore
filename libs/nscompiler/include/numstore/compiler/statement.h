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
 *   Statement structure and management. Defines the statement type that
 *   contains a query and associated memory space for query variables,
 *   including creation and destruction functions.
 */

// compiler
#include <numstore/compiler/queries/query.h>

struct statement
{
  struct query q;

  /* Room for the query variables */
  struct lalloc qspace;
  u8 query_space[2048];
};

struct statement *statement_create (error *e);
void statement_free (struct statement *s);
