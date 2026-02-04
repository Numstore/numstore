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
 *   Implements types.h. Provides unified type system operations including validation,
 *   serialization, deserialization, byte size calculations, and type equality checks.
 */

#include "numstore/core/error.h"
#include <numstore/types/types.h>

#include <numstore/core/assert.h>
#include <numstore/core/random.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/types/prim.h>

// core
DEFINE_DBG_ASSERT (
    struct type, unchecked_type, t,
    {
      ASSERT (t);
    })

DEFINE_DBG_ASSERT (
    struct type, valid_type, t,
    {
      ASSERT (t);
      ASSERT (type_validate (t, NULL) == SUCCESS);
    })

err_t
type_validate (const struct type *t, error *e)
{
  DBG_ASSERT (unchecked_type, t);
  switch (t->type)
    {
    case T_PRIM:
      {
        return prim_t_validate (&t->p, e);
      }
    case T_STRUCT:
      {
        return struct_t_validate (&t->st, e);
      }
    case T_UNION:
      {
        return union_t_validate (&t->un, e);
      }
    case T_ENUM:
      {
        return enum_t_validate (&t->en, e);
      }
    case T_SARRAY:
      {
        return sarray_t_validate (&t->sa, e);
      }
    default:
      {
        UNREACHABLE ();
        return -1;
      }
    }
}

i32
type_snprintf (char *str, u32 size, struct type *t)
{
  DBG_ASSERT (valid_type, t);

  switch (t->type)
    {
    case T_PRIM:
      {
        return prim_t_snprintf (str, size, &t->p);
      }
    case T_STRUCT:
      {
        return struct_t_snprintf (str, size, &t->st);
      }
    case T_UNION:
      {
        return union_t_snprintf (str, size, &t->un);
      }
    case T_ENUM:
      {
        return enum_t_snprintf (str, size, &t->en);
      }
    case T_SARRAY:
      {
        return sarray_t_snprintf (str, size, &t->sa);
      }
    default:
      {
        UNREACHABLE ();
        return -1;
      }
    }
}

u32
type_byte_size (const struct type *t)
{
  DBG_ASSERT (valid_type, t);

  switch (t->type)
    {
    case T_PRIM:
      {
        return prim_t_byte_size (&t->p);
      }

    case T_STRUCT:
      {
        return struct_t_byte_size (&t->st);
      }

    case T_UNION:
      {
        return union_t_byte_size (&t->un);
      }

    case T_ENUM:
      {
        return enum_t_byte_size (&t->en);
      }

    case T_SARRAY:
      {
        return sarray_t_byte_size (&t->sa);
      }

    default:
      {
        UNREACHABLE ();
        return 0;
      }
    }
}

u32
type_get_serial_size (const struct type *t)
{
  DBG_ASSERT (valid_type, t);

  /**
   * LABEL TYPE
   */
  u32 ret = sizeof (u8);

  switch (t->type)
    {
    case T_PRIM:
      {
        return ret + sizeof (u8);
      }
    case T_STRUCT:
      {
        return ret + struct_t_get_serial_size (&t->st);
      }
    case T_UNION:
      {
        return ret + union_t_get_serial_size (&t->un);
      }
    case T_ENUM:
      {
        return ret + enum_t_get_serial_size (&t->en);
      }
    case T_SARRAY:
      {
        return ret + sarray_t_get_serial_size (&t->sa);
      }
    default:
      {
        UNREACHABLE ();
        return 0;
      }
    }
}

void
type_serialize (struct serializer *dest, const struct type *src)
{
  DBG_ASSERT (valid_type, src);
  bool ret;

  u8 type_val = (u8)src->type;
  ret = srlizr_write (dest, &type_val, sizeof (u8));
  ASSERT (ret);

  switch (src->type)
    {
    case T_PRIM:
      {
        prim_t_serialize (dest, &src->p);
        break;
      }
    case T_STRUCT:
      {
        struct_t_serialize (dest, &src->st);
        break;
      }
    case T_UNION:
      {
        union_t_serialize (dest, &src->un);
        break;
      }
    case T_ENUM:
      {
        enum_t_serialize (dest, &src->en);
        break;
      }
    case T_SARRAY:
      {
        sarray_t_serialize (dest, &src->sa);
        break;
      }
    default:
      {
        UNREACHABLE ();
        break;
      }
    }
}

err_t
type_deserialize (struct type *dest, struct deserializer *src, struct chunk_alloc *alloc, error *e)
{
  u8 header;
  bool ret = dsrlizr_read (&header, sizeof (u8), src);
  dest->type = (enum type_t)header;

  switch (header)
    {
    case T_PRIM:
      {
        return prim_t_deserialize (&dest->p, src, e);
      }
    case T_STRUCT:
      {
        return struct_t_deserialize (&dest->st, src, alloc, e);
      }
    case T_UNION:
      {
        return union_t_deserialize (&dest->un, src, alloc, e);
      }
    case T_ENUM:
      {
        return enum_t_deserialize (&dest->en, src, alloc, e);
      }
    case T_SARRAY:
      {
        return sarray_t_deserialize (&dest->sa, src, alloc, e);
      }
    default:
      {
        return error_causef (e, ERR_INTERP, "Unknown type code: %d", ret);
      }
    }
}

err_t
type_random (struct type *dest, struct chunk_alloc *alloc, u32 depth, error *e)
{
  ASSERT (dest);

  if (depth == 0)
    {
      dest->type = T_PRIM;
      dest->p = prim_t_random ();
      return SUCCESS;
    }

  static const enum type_t weighted[] = {
    T_PRIM, T_PRIM, T_PRIM, T_PRIM,
    T_ENUM, T_STRUCT, T_UNION, T_SARRAY
  };

  dest->type = weighted[randu32r (0, arrlen (weighted))];

  switch (dest->type)
    {
    case T_PRIM:
      {
        dest->p = prim_t_random ();
        return SUCCESS;
      }

    case T_ENUM:
      {
        err_t_wrap (enum_t_random (&dest->en, alloc, e), e);
        break;
      }

    case T_STRUCT:
      {
        err_t_wrap (struct_t_random (&dest->st, alloc, depth - 1, e), e);
        break;
      }

    case T_UNION:
      {
        err_t_wrap (union_t_random (&dest->un, alloc, depth - 1, e), e);
        break;
      }

    case T_SARRAY:
      {
        err_t_wrap (sarray_t_random (&dest->sa, alloc, depth - 1, e), e);
        break;
      }

    default:
      {
        return error_causef (e, ERR_NOMEM, "Invalid type tag");
      }
    }
  return SUCCESS;
}

bool
type_equal (const struct type *left, const struct type *right)
{
  if (left->type != right->type)
    {
      return false;
    }

  switch (left->type)
    {
    case T_PRIM:
      {
        return left->p == right->p;
      }
    case T_STRUCT:
      {
        return struct_t_equal (&left->st, &right->st);
      }
    case T_UNION:
      {
        return union_t_equal (&left->un, &right->un);
      }
    case T_ENUM:
      {
        return enum_t_equal (&left->en, &right->en);
      }
    case T_SARRAY:
      {
        return sarray_t_equal (&left->sa, &right->sa);
      }
    default:
      {
        UNREACHABLE ();
      }
    }
}

err_t
i_log_type (struct type t, error *e)
{
  i32 len = type_snprintf (NULL, 0, &t);
  if (len < 0)
    {
      return error_causef (e, ERR_IO, "Failed to call snprintf\n");
    }

  char *dest = i_malloc (len + 1, sizeof *dest, e);
  if (dest == NULL)
    {
      return error_causef (e, ERR_NOMEM, "Failed to allocate dest for log type\n");
    }

  len = type_snprintf (dest, len + 1, &t);
  if (len < 0)
    {
      i_free (dest);
      return error_causef (e, ERR_IO, "Failed to call snprintf\n");
    }

  i_log_info ("%.*s\n", len, dest);
  i_free (dest);

  return SUCCESS;
}
