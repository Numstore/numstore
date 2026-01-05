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
 *   Implements queries/query.h. Provides query logging and equality comparison
 *   functions that dispatch to type-specific implementations for all query
 *   types.
 */

#include <numstore/compiler/queries/query.h>

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/core/strings_utils.h>
#include <numstore/intf/os.h>
#include <numstore/test/testing.h>

void
i_log_query (struct query *q)
{
  switch (q->type)
    {
    case QT_CREATE:
      {
        i_log_create (&q->create);
        break;
      }
    case QT_DELETE:
      {
        i_log_delete (&q->delete);
        break;
      }
    case QT_SHELL_INSERT:
      {
        i_log_net_insert (&q->net_insert);
        break;
      }
    case QT_NET_OPEN:
    case QT_NET_CLOSE:
    case QT_NET_INSERT:
      {
        UNIMPLEMENTED ();
      }
    default:
      {
        UNREACHABLE ();
      }
    }
}

bool
query_equal (const struct query *left, const struct query *right)
{
  if (left->type != right->type)
    {
      return false;
    }

  switch (left->type)
    {
    case QT_CREATE:
      {
        return create_query_equal (&left->create, &right->create);
      }
    case QT_DELETE:
      {
        return delete_query_equal (&left->delete, &right->delete);
      }
    case QT_SHELL_INSERT:
      {
        return net_insert_query_equal (&left->net_insert, &right->net_insert);
      }
    case QT_NET_OPEN:
    case QT_NET_CLOSE:
    case QT_NET_INSERT:
      {
        UNIMPLEMENTED ();
      }
    default:
      {
        UNREACHABLE ();
      }
    }
}
