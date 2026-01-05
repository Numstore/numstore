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
 *   Top-level query types and structures. Defines the query union that
 *   encompasses all query types (CREATE, DELETE, shell INSERT, network
 *   operations) with logging and equality functions.
 */

// numstore
#include <numstore/compiler/network/close.h>
#include <numstore/compiler/network/insert.h>
#include <numstore/compiler/network/open.h>
#include <numstore/compiler/queries/create.h>
#include <numstore/compiler/queries/delete.h>
#include <numstore/compiler/shell/insert.h>
#include <numstore/types/types.h>

///////////////////////////
// QUERY
enum query_t
{
  // Command and Control
  QT_CREATE,
  QT_DELETE,

  // Shell Queries
  QT_SHELL_INSERT,

  // Network Queries
  QT_NET_OPEN,
  QT_NET_CLOSE,
  QT_NET_INSERT,
};

/// QUERY
struct query
{
  enum query_t type;

  union
  {
    // Command and Control
    struct create_query create;
    struct delete_query delete;

    // Shell Queries
    struct shell_insert_query shell_insert;

    // Network Queries
    struct net_open_query open;
    struct net_close_query close;
    struct net_insert_query net_insert;
  };
};

void i_log_query (struct query *q);
bool query_equal (
    const struct query *left,
    const struct query *right);
