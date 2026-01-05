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
 *   Implements network/insert.h. Provides builder pattern implementation for
 *   constructing network INSERT queries with validation of variable names,
 *   ports, and start offsets.
 */

#include <numstore/compiler/network/insert.h>

#include <numstore/core/assert.h>
#include <numstore/core/strings_utils.h>
#include <numstore/intf/logging.h>

// Core
DEFINE_DBG_ASSERT (
    struct net_insert_query,
    net_insert_query, i,
    {
      ASSERT (i);
    })

void
i_log_net_insert (struct net_insert_query *q)
{
  DBG_ASSERT (net_insert_query, q);
  i_log_info ("Inserting to: %.*s at index: %lu. Value:\n", q->vname.len, q->vname.data, q->start);
}

bool
net_insert_query_equal (const struct net_insert_query *left, const struct net_insert_query *right)
{
  if (!string_equal (left->vname, right->vname))
    {
      return false;
    }
  if (left->port != right->port)
    {
      return false;
    }
  if (left->start != right->start)
    {
      return false;
    }
  return true;
}

DEFINE_DBG_ASSERT (
    struct net_insert_builder,
    net_insert_builder, c,
    {
      ASSERT (c);
    })

struct net_insert_builder
inb_create (void)
{
  struct net_insert_builder ret = {
    .vname = (struct string){ 0 },
    .port = -1,
    .start = 0,

    .got_vname = false,
    .got_port = false,
    .got_start = false,
  };

  DBG_ASSERT (net_insert_builder, &ret);

  return ret;
}

err_t
inb_accept_string (struct net_insert_builder *b, const struct string vname, error *e)
{
  DBG_ASSERT (net_insert_builder, b);
  if (b->got_vname)
    {
      return error_causef (
          e, ERR_INTERP,
          "Insert Builder: "
          "Already have a variable name");
    }

  if (vname.len == 0)
    {
      return error_causef (
          e, ERR_INTERP,
          "Insert Builder: "
          "Length of variable names must be > 0");
    }

  b->vname = vname;
  b->got_vname = true;

  return SUCCESS;
}

err_t
inb_accept_port (struct net_insert_builder *b, int port, error *e)
{
  DBG_ASSERT (net_insert_builder, b);
  if (b->got_port)
    {
      return error_causef (
          e, ERR_INTERP,
          "Insert Builder: "
          "Already have a port");
    }

  b->port = port;
  b->got_port = true;

  return SUCCESS;
}

err_t
inb_accept_start (struct net_insert_builder *b, b_size start, error *e)
{
  DBG_ASSERT (net_insert_builder, b);
  if (b->got_start)
    {
      return error_causef (
          e, ERR_INTERP,
          "Insert Builder: "
          "Already have a start index");
    }

  b->start = start;
  b->got_start = true;

  return SUCCESS;
}

err_t
inb_build (struct net_insert_query *dest, struct net_insert_builder *b, error *e)
{
  DBG_ASSERT (net_insert_builder, b);
  ASSERT (dest);

  if (!b->got_port)
    {
      return error_causef (
          e, ERR_INTERP,
          "Insert Builder: "
          "Need a port to build a net_insert query");
    }

  if (!b->got_vname)
    {
      return error_causef (
          e, ERR_INTERP,
          "Insert Builder: "
          "Need a variable name to build a net_insert query");
    }

  dest->vname = b->vname;
  dest->port = b->port;
  dest->start = b->start;

  return SUCCESS;
}
