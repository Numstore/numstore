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
 *   Network insert query structures and builder pattern. Defines query type
 *   for inserting data to variables over network connections, including port,
 *   variable name, start offset, and length.
 */

// numstore
#include <numstore/types/types.h>

///////////////////////////
// NET INSERT

struct net_insert_query
{
  struct string vname;
  int port;
  b_size start;
  b_size len;
};

void i_log_net_insert (struct net_insert_query *q);
bool net_insert_query_equal (
    const struct net_insert_query *left,
    const struct net_insert_query *right);

struct net_insert_builder
{
  struct string vname;
  int port;
  b_size start;

  bool got_vname;
  bool got_port;
  bool got_start;
};

struct net_insert_builder inb_create (void);

err_t inb_accept_string (struct net_insert_builder *a, const struct string vname, error *e);
err_t inb_accept_port (struct net_insert_builder *a, int port, error *e);
err_t inb_accept_start (struct net_insert_builder *a, b_size start, error *e);

err_t inb_build (struct net_insert_query *dest, struct net_insert_builder *b, error *e);
