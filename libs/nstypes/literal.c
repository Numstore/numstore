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
 *   Implements literal.h. Provides implementation of literal operations,
 *   object and array builders, and arithmetic/logical operators for all
 *   literal types.
 */

#include <numstore/types/literal.h>

#include <numstore/core/assert.h>
#include <numstore/core/math.h>
#include <numstore/core/string.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/stdlib.h>
#include <numstore/test/testing.h>

static const char *OBJECT_BUILDER_TAG = "Object Value Builder";

/////////////////////////
// Simple constructors for the other types

HEADER_FUNC struct literal
literal_string_create (struct string str)
{
  return (struct literal){
    .str = str,
    .type = LT_STRING,
  };
}

HEADER_FUNC struct literal
literal_integer_create (i64 integer)
{
  return (struct literal){
    .type = LT_INTEGER,
    .integer = integer,
  };
}

HEADER_FUNC struct literal
literal_decimal_create (f128 decimal)
{
  return (struct literal){
    .type = LT_DECIMAL,
    .decimal = decimal,
  };
}

HEADER_FUNC struct literal
literal_complex_create (cf128 cplx)
{
  return (struct literal){
    .type = LT_COMPLEX,
    .cplx = cplx,
  };
}

HEADER_FUNC struct literal
literal_bool_create (bool bl)
{
  return (struct literal){
    .type = LT_BOOL,
    .bl = bl,
  };
}

////////////////////////////////////////////////////////////
// OBJECT AND ARRAY VALUE TYPES

i32
object_t_snprintf (char *str, u32 size, const struct object *st)
{
  (void)str;
  (void)size;
  (void)st;
  return -1;
}

bool
object_equal (const struct object *left, const struct object *right)
{
  if (left->len != right->len)
    {
      return false;
    }

  for (u32 i = 0; i < left->len; ++i)
    {
      if (!string_equal (left->keys[i], right->keys[i]))
        {
          return false;
        }
      if (!literal_equal (&left->literals[i], &right->literals[i]))
        {
          return false;
        }
    }

  return true;
}

err_t
object_plus (struct object *dest, const struct object *right, struct lalloc *alloc, error *e)
{
  /* Check for duplicate keys */
  const struct string *duplicate = strings_are_disjoint (dest->keys, dest->len, right->keys, right->len);
  if (duplicate != NULL)
    {
      return error_causef (
          e, ERR_INTERP,
          "Cannot merge two objects with duplicate keys: %.*s",
          duplicate->len, duplicate->data);
    }

  u32 len = dest->len + right->len;

  struct string *keys = lmalloc (alloc, len, sizeof *keys, e);
  if (keys == NULL)
    {
      return e->cause_code;
    }
  struct literal *literals = lmalloc (alloc, len, sizeof *literals, e);
  if (literals == NULL)
    {
      return e->cause_code;
    }

  /* Copy over literals */
  i_memcpy (literals, dest->literals, dest->len * sizeof *dest->literals);
  i_memcpy (literals + dest->len, right->literals, right->len * sizeof *right->literals);

  /* Copy over keys */
  i_memcpy (keys, dest->keys, dest->len * sizeof *dest->keys);
  i_memcpy (keys + dest->len, right->keys, right->len * sizeof *right->keys);

  dest->len = len;

  return SUCCESS;
}

bool
array_equal (const struct array *left, const struct array *right)
{
  if (left->len != right->len)
    {
      return false;
    }

  for (u32 i = 0; i < left->len; ++i)
    {
      if (!literal_equal (&left->literals[i], &right->literals[i]))
        {
          return false;
        }
    }

  return true;
}

err_t
array_plus (struct array *dest, const struct array *right, struct lalloc *alloc, error *e)
{
  u32 len = dest->len + right->len;
  struct literal *literals = lmalloc (alloc, len, sizeof *literals, e);
  if (literals == NULL)
    {
      return e->cause_code;
    }

  i_memcpy (literals, dest->literals, dest->len * sizeof *dest->literals);
  i_memcpy (literals + dest->len, right->literals, right->len * sizeof *right->literals);
  dest->len = len;

  return SUCCESS;
}

////////////////////////////////////////////////////////////
// VALUE

const char *
literal_t_tostr (enum literal_t t)
{
  switch (t)
    {
      /* Composite */
      case_ENUM_RETURN_STRING (LT_OBJECT);
      case_ENUM_RETURN_STRING (LT_ARRAY);

      /* Simple */
      case_ENUM_RETURN_STRING (LT_STRING);
      case_ENUM_RETURN_STRING (LT_INTEGER);
      case_ENUM_RETURN_STRING (LT_DECIMAL);
      case_ENUM_RETURN_STRING (LT_COMPLEX);
      case_ENUM_RETURN_STRING (LT_BOOL);
    }

  UNREACHABLE ();
}

bool
literal_equal (const struct literal *left, const struct literal *right)
{
  if (left->type != right->type)
    {
      return false;
    }

  switch (left->type)
    {
    /* Composite */
    case LT_OBJECT:
      {
        return object_equal (&left->obj, &right->obj);
      }
    case LT_ARRAY:
      {
        return array_equal (&left->arr, &right->arr);
      }

    /* Simple */
    case LT_STRING:
      {
        return string_equal (left->str, right->str);
      }
    case LT_INTEGER:
      {
        return left->integer == right->integer;
      }
    case LT_DECIMAL:
      {
        return left->decimal == right->decimal;
      }
    case LT_COMPLEX:
      {
        return left->cplx == right->cplx;
      }
    case LT_BOOL:
      {
        return left->bl == right->bl;
      }
    }
  UNREACHABLE ();
}

////////////////////////////////////////////////////////////
// OBJECT / ARRAY BUILDERS

DEFINE_DBG_ASSERT (struct object_builder, object_builder, a,
                   {
                     ASSERT (a);
                   })

struct object_builder
objb_create (struct lalloc *alloc, struct lalloc *dest)
{
  struct object_builder builder = {
    .head = NULL,
    .klen = 0,
    .tlen = 0,
    .work = alloc,
    .dest = dest,
  };
  DBG_ASSERT (object_builder, &builder);
  return builder;
}

static bool
object_has_key_been_used (const struct object_builder *ub, struct string key)
{
  for (struct llnode *it = ub->head; it; it = it->next)
    {
      struct object_llnode *kn = container_of (it, struct object_llnode, link);
      if (string_equal (kn->key, key))
        {
          return true;
        }
    }
  return false;
}

err_t
objb_accept_string (struct object_builder *ub, const struct string key, error *e)
{
  DBG_ASSERT (object_builder, ub);

  /* Check for duplicate keys */
  if (object_has_key_been_used (ub, key))
    {
      return error_causef (
          e, ERR_INTERP,
          "%s: Key: %.*s has already been used",
          OBJECT_BUILDER_TAG, key.len, key.data);
    }

  /* Find where to insert this new key in the linked list */
  struct llnode *slot = llnode_get_n (ub->head, ub->klen);
  struct object_llnode *node;
  if (slot)
    {
      node = container_of (slot, struct object_llnode, link);
    }
  else
    {
      /* Allocate new node onto allocator */
      node = lmalloc (ub->work, 1, sizeof *node, e);
      if (!node)
        {
          return e->cause_code;
        }
      llnode_init (&node->link);
      node->v = (struct literal){ 0 };

      /* Set the head if it doesn't exist */
      if (!ub->head)
        {
          ub->head = &node->link;
        }

      /* Otherwise, append to the list */
      else
        {
          list_append (&ub->head, &node->link);
        }
    }

  /* Create the node */
  node->key = key;
  ub->klen++;

  return SUCCESS;
}

err_t
objb_accept_literal (struct object_builder *ub, struct literal t, error *e)
{
  DBG_ASSERT (object_builder, ub);

  struct llnode *slot = llnode_get_n (ub->head, ub->tlen);
  struct object_llnode *node;
  if (slot)
    {
      node = container_of (slot, struct object_llnode, link);
    }
  else
    {
      node = lmalloc (ub->work, 1, sizeof *node, e);
      if (!node)
        {
          return e->cause_code;
        }
      llnode_init (&node->link);
      node->key = (struct string){ 0 };
      if (!ub->head)
        {
          ub->head = &node->link;
        }
      else
        {
          list_append (&ub->head, &node->link);
        }
    }

  node->v = t;
  ub->tlen++;
  return SUCCESS;
}

static err_t
objb_build_common (
    struct string **out_keys,
    struct literal **out_types,
    u16 *out_len,
    struct object_builder *ub,
    struct lalloc *onto,
    error *e)
{
  if (ub->klen == 0)
    {
      return error_causef (
          e, ERR_INTERP,
          "%s: Expecting at least one key", OBJECT_BUILDER_TAG);
    }
  if (ub->klen != ub->tlen)
    {
      return error_causef (
          e, ERR_INTERP,
          "%s: Must have same number of keys and literals", OBJECT_BUILDER_TAG);
    }

  *out_keys = lmalloc (onto, ub->klen, sizeof **out_keys, e);
  if (!*out_keys)
    {
      return e->cause_code;
    }

  *out_types = lmalloc (onto, ub->tlen, sizeof **out_types, e);
  if (!*out_types)
    {
      return e->cause_code;
    }

  size_t i = 0;
  for (struct llnode *it = ub->head; it; it = it->next)
    {
      struct object_llnode *kn = container_of (it, struct object_llnode, link);
      (*out_keys)[i] = kn->key;
      (*out_types)[i] = kn->v;
      i++;
    }
  *out_len = ub->klen;
  return SUCCESS;
}

err_t
objb_build (struct object *dest, struct object_builder *ub, error *e)
{
  struct string *keys = NULL;
  struct literal *literals = NULL;
  u16 len = 0;

  err_t_wrap (objb_build_common (&keys, &literals, &len, ub, ub->dest, e), e);

  ASSERT (keys);
  ASSERT (literals);

  dest->keys = keys;
  dest->literals = literals;
  dest->len = len;
  return SUCCESS;
}

DEFINE_DBG_ASSERT (struct array_builder, array_builder, a,
                   {
                     ASSERT (a);
                   })

struct array_builder
arb_create (struct lalloc *work, struct lalloc *dest)
{
  struct array_builder builder = {
    .head = NULL,
    .work = work,
    .dest = dest,
  };

  DBG_ASSERT (array_builder, &builder);

  return builder;
}

err_t
arb_accept_literal (struct array_builder *o, struct literal v, error *e)
{
  DBG_ASSERT (array_builder, o);

  u16 idx = (u16)list_length (o->head);
  struct llnode *slot = llnode_get_n (o->head, idx);
  struct array_llnode *node;

  if (slot)
    {
      node = container_of (slot, struct array_llnode, link);
    }
  else
    {
      node = lmalloc (o->work, 1, sizeof *node, e);
      if (!node)
        {
          return e->cause_code;
        }
      llnode_init (&node->link);
      if (!o->head)
        {
          o->head = &node->link;
        }
      else
        {
          list_append (&o->head, &node->link);
        }
    }

  node->v = v;
  return SUCCESS;
}

err_t
arb_build (struct array *dest, struct array_builder *b, error *e)
{
  DBG_ASSERT (array_builder, b);
  ASSERT (dest);

  u16 length = (u16)list_length (b->head);

  struct literal *literals = lmalloc (b->dest, length, sizeof *literals, e);
  if (!literals)
    {
      return e->cause_code;
    }

  u16 i = 0;
  for (struct llnode *it = b->head; it; it = it->next)
    {
      struct array_llnode *dn = container_of (it, struct array_llnode, link);
      literals[i++] = dn->v;
    }

  dest->len = length;
  dest->literals = literals;

  return SUCCESS;
}

void
i_log_literal (struct literal *v)
{
  switch (v->type)
    {
    case LT_OBJECT:
      {
        i_log_info ("====== OBJECT: \n");
        for (u32 i = 0; i < v->obj.len; ++i)
          {
            i_log_info ("Key: %.*s. Value: \n", v->obj.keys[i].len, v->obj.keys[i].data);
            i_log_literal (&v->obj.literals[i]);
          }
        i_log_info ("====== DONE \n");
        return;
      }
    case LT_ARRAY:
      {
        i_log_info ("====== ARRAY: \n");
        for (u32 i = 0; i < v->arr.len; ++i)
          {
            i_log_literal (&v->arr.literals[i]);
          }
        i_log_info ("====== DONE \n");
        return;
      }
    case LT_STRING:
      {
        i_log_info ("%.*s\n", v->str.len, v->str.data);
        return;
      }
    case LT_INTEGER:
      {
        i_log_info ("%d\n", v->integer);
        return;
      }
    case LT_DECIMAL:
      {
        i_log_info ("%f\n", v->decimal);
        return;
      }
    case LT_COMPLEX:
      {
        i_log_info ("%f %f\n", i_creal_64 (v->cplx), i_cimag_64 (v->cplx));
        return;
      }
    case LT_BOOL:
      {
        if (v->bl)
          {
            i_log_info ("TRUE\n");
          }
        else
          {
            i_log_info ("FALSE\n");
          }
        return;
      }
    }
}
