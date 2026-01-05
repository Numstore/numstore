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
 *   CREATE query structures and builder pattern. Defines query type for
 *   creating variables with specified names and types, including builder
 *   functions for constructing queries.
 */

// numstore
#include <numstore/types/types.h>

///////////////////////////
// CREATE
struct create_query
{
  struct string vname;
  struct type type;
};

void i_log_create (struct create_query *q);
bool create_query_equal (const struct create_query *left, const struct create_query *right);

struct create_builder
{
  struct string vname;
  struct type type;
  bool got_type;
  bool got_vname;
};

struct create_builder crb_create (void);
err_t crb_accept_string (struct create_builder *b, const struct string vname, error *e);
err_t crb_accept_type (struct create_builder *b, struct type t, error *e);
err_t crb_build (struct create_query *dest, struct create_builder *b, error *e);
