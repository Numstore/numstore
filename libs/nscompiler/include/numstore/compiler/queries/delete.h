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
 *   DELETE query structures and builder pattern. Defines query type for
 *   deleting variables by name, including builder functions for constructing
 *   queries.
 */

// numstore
#include <numstore/types/types.h>

///////////////////////////
// DELETE
struct delete_query
{
  struct string vname; /* Variable to delete */
};

void i_log_delete (struct delete_query *q);
bool delete_query_equal (const struct delete_query *left, const struct delete_query *right);

struct delete_builder
{
  struct string vname;
  bool got_vname;
};

struct delete_builder dltb_create (void);
err_t dltb_accept_string (struct delete_builder *b, const struct string vname, error *e);
err_t dltb_build (struct delete_query *dest, struct delete_builder *b, error *e);
