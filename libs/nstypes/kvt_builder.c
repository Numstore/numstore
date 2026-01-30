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
 *   Implements kvt_builder.h. Provides builder pattern implementation for constructing
 *   struct and union types by accepting keys and types incrementally.
 */

#include "numstore/core/chunk_alloc.h"
#include "numstore/core/error.h"
#include <numstore/types/kvt_builder.h>

#include <numstore/core/assert.h>
#include <numstore/core/strings_utils.h>
#include <numstore/intf/logging.h>
#include <numstore/test/testing.h>
#include <numstore/types/types.h>

DEFINE_DBG_ASSERT (
    struct kvt_builder, kvt_builder, s,
    {
      ASSERT (s);
      ASSERT (s->klen <= 10);
      ASSERT (s->tlen <= 10);
    })

void
kvb_create (struct kvt_builder *dest, struct chunk_alloc *temp, struct chunk_alloc *persistent)
{
  *dest = (struct kvt_builder){
    .head = NULL,
    .klen = 0,
    .tlen = 0,
    .temp = temp,
    .persistent = persistent,
  };
  DBG_ASSERT (kvt_builder, dest);
}

static bool
kvt_has_key_been_used (const struct kvt_builder *ub, struct string key)
{
  for (struct llnode *it = ub->head; it; it = it->next)
    {
      struct kv_llnode *kn = container_of (it, struct kv_llnode, link);
      if (string_equal (kn->key, key))
        {
          return true;
        }
    }
  return false;
}

err_t
kvb_accept_key (struct kvt_builder *ub, struct string key, error *e)
{
  DBG_ASSERT (kvt_builder, ub);
  i_log_info ("Accepting key: %.*s\n", (u32)key.len, key.data);

  /* Check for duplicate keys */
  if (kvt_has_key_been_used (ub, key))
    {
      return error_causef (
          e, ERR_INTERP,
          "Key: %.*s has already been used",
          key.len, key.data);
    }

  // Copy key data to persistent memory
  key.data = chunk_alloc_move_mem (ub->persistent, key.data, key.len, e);
  if (key.data == NULL)
    {
      return e->cause_code;
    }

  /* Find where to insert this new key in the linked list */
  struct llnode *slot = llnode_get_n (ub->head, ub->klen);
  struct kv_llnode *node;
  if (slot)
    {
      node = container_of (slot, struct kv_llnode, link);
    }
  else
    {
      /* Allocate new node onto tempator */
      node = chunk_malloc (ub->temp, 1, sizeof *node, e);
      if (!node)
        {
          return e->cause_code;
        }
      llnode_init (&node->link);
      node->value = (struct type){ 0 };

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

  // Create the node
  node->key = key;
  ub->klen++;

  return SUCCESS;
}

err_t
kvb_accept_type (struct kvt_builder *ub, struct type t, error *e)
{
  DBG_ASSERT (kvt_builder, ub);

  struct llnode *slot = llnode_get_n (ub->head, ub->tlen);
  struct kv_llnode *node;
  if (slot)
    {
      node = container_of (slot, struct kv_llnode, link);
    }
  else
    {
      node = chunk_malloc (ub->temp, 1, sizeof *node, e);
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

  node->value = t;
  ub->tlen++;
  return SUCCESS;
}

static err_t
kvb_build_common (
    struct string **out_keys,
    struct type **out_types,
    u16 *out_len,
    struct kvt_builder *ub,
    struct chunk_alloc *onto,
    error *e)
{
  if (ub->klen == 0)
    {
      return error_causef (
          e, ERR_INTERP,
          "Expecting at least one key");
    }
  if (ub->klen != ub->tlen)
    {
      return error_causef (
          e, ERR_INTERP,
          "Must have same number of keys and values");
    }

  *out_keys = chunk_malloc (onto, ub->klen, sizeof **out_keys, e);
  if (!*out_keys)
    {
      return e->cause_code;
    }

  *out_types = chunk_malloc (onto, ub->tlen, sizeof **out_types, e);
  if (!*out_types)
    {
      return e->cause_code;
    }

  size_t i = 0;
  for (struct llnode *it = ub->head; it; it = it->next)
    {
      struct kv_llnode *kn = container_of (it, struct kv_llnode, link);
      (*out_keys)[i] = kn->key;
      (*out_types)[i] = kn->value;
      i++;
    }
  *out_len = ub->klen;
  return SUCCESS;
}

err_t
kvb_union_t_build (struct union_t *dest, struct kvt_builder *ub, error *e)
{
  struct string *keys = NULL;
  struct type *types = NULL;
  u16 len = 0;

  err_t_wrap (kvb_build_common (&keys, &types, &len, ub, ub->persistent, e), e);

  ASSERT (keys);
  ASSERT (types);
  ASSERT (len > 0);

  dest->keys = keys;
  dest->types = types;
  dest->len = len;

  return SUCCESS;
}

err_t
kvb_struct_t_build (struct struct_t *dest, struct kvt_builder *ub, error *e)
{
  struct string *keys = NULL;
  struct type *types = NULL;
  u16 len = 0;

  err_t_wrap (kvb_build_common (&keys, &types, &len, ub, ub->persistent, e), e);

  ASSERT (keys);
  ASSERT (types);
  ASSERT (len > 0);

  dest->keys = keys;
  dest->types = types;
  dest->len = len;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, kvt_builder)
{
  error err = error_create ();
  u8 _temp[2048];
  u8 _persistent[2048];

  /* provide two fixed‑size tempators for nodes + strings */
  struct chunk_alloc persistent;
  chunk_alloc_create_default (&persistent);

  /* 0. freshly‑created builder must be clean */
  struct kvt_builder kb;
  kvb_create (&kb, &persistent, &persistent);
  test_assert_int_equal (kb.klen, 0);
  test_assert_int_equal (kb.tlen, 0);
  test_fail_if (kb.head != NULL);

  /* 1. accept first key "id" */
  struct string key_id = strfcstr ("id");
  test_assert_int_equal (kvb_accept_key (&kb, key_id, &err), SUCCESS);

  /* 2. duplicate key "id" must fail */
  test_assert_int_equal (kvb_accept_key (&kb, key_id, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 3. accept a type for that key (u32) */
  struct type t_u32 = (struct type){ .type = T_PRIM, .p = U32 };
  test_assert_int_equal (kvb_accept_type (&kb, t_u32, &err), SUCCESS);

  /* 4. accept second key/value pair ("name", string) */
  struct string key_name = strfcstr ("name");
  test_assert_int_equal (kvb_accept_key (&kb, key_name, &err), SUCCESS);
  struct type t_str = (struct type){ .type = T_PRIM, .p = I32 };
  test_assert_int_equal (kvb_accept_type (&kb, t_str, &err), SUCCESS);

  /* 6. mismatched key/type counts → build must fail */
  struct string key_extra = strfcstr ("extra");
  test_assert_int_equal (kvb_accept_key (&kb, key_extra, &err), SUCCESS); /* klen=3, tlen=2 */
  struct struct_t st_fail = { 0 };
  test_assert_int_equal (kvb_struct_t_build (&st_fail, &kb, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 7. add matching type so counts align */
  struct type t_f32 = (struct type){ .type = T_PRIM, .p = F32 };
  test_assert_int_equal (kvb_accept_type (&kb, t_f32, &err), SUCCESS);

  /* 8. successful struct build */
  struct struct_t st = { 0 };
  test_assert_int_equal (kvb_struct_t_build (&st, &kb, &err), SUCCESS);
  test_assert_int_equal (st.len, 3);
  test_fail_if_null (st.keys);
  test_fail_if_null (st.types);

  /* 9. ensure key order preserved (id, name, extra) */
  test_assert_int_equal (string_equal (st.keys[0], key_id), true);
  test_assert_int_equal (string_equal (st.keys[1], key_name), true);
  test_assert_int_equal (string_equal (st.keys[2], key_extra), true);

  /* 10. ensure type mapping correct */
  test_assert_int_equal (st.types[0].p, t_u32.p);
  test_assert_int_equal (st.types[1].p, t_str.p);
  test_assert_int_equal (st.types[2].p, t_f32.p);

  /* 11. union build with same builder should also succeed */
  struct union_t un = { 0 };
  test_assert_int_equal (kvb_union_t_build (&un, &kb, &err), SUCCESS);
  test_assert_int_equal (un.len, st.len);
}
#endif
