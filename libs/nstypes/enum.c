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
 *   Implements enum.h. Provides enumeration type operations including validation,
 *   serialization, deserialization, random generation, and builder functions.
 */

#include "numstore/core/chunk_alloc.h"
#include "numstore/core/error.h"
#include <numstore/types/enum.h>

#include <numstore/core/assert.h>
#include <numstore/core/random.h>
#include <numstore/core/string.h>
#include <numstore/intf/stdlib.h>
#include <numstore/test/testing.h>
#include <numstore/types/types.h>

// core
//////////////////////////////
/// MODEL

DEFINE_DBG_ASSERT (
    struct enum_t, unchecked_enum_t, e,
    {
      ASSERT (e);
      ASSERT (e->keys);
    })

DEFINE_DBG_ASSERT (
    struct enum_t, valid_enum_t, e,
    {
      error e_ = error_create ();
      ASSERT (enum_t_validate (e, &e_) == SUCCESS);
    })

err_t
enum_t_validate (const struct enum_t *en, error *e)
{
  DBG_ASSERT (unchecked_enum_t, en);

  if (en->len == 0)
    {
      return error_causef (e, ERR_INTERP, "Number of keys must be > 0");
    }
  for (u32 i = 0; i < en->len; ++i)
    {
      if (en->keys[i].len == 0)
        {
          return error_causef (e, ERR_INTERP, "Key length must be > 0");
        }
      ASSERT (en->keys[i].data);
    }

  if (!strings_all_unique (en->keys, en->len))
    {
      return error_causef (e, ERR_INTERP, "Duplicate key");
    }

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, enum_t_validate)
{
  error err = error_create ();

  /* 1.1 happy path – two unique keys */
  struct string keys_ok[2] = { strfcstr ("FOO"), strfcstr ("BAR") };
  struct enum_t en_ok = { .keys = keys_ok, .len = 2 };
  test_assert_int_equal (enum_t_validate (&en_ok, &err), SUCCESS);

  /* 1.3 duplicate key - ERR_INTERP */
  struct string keys_dup[2] = { strfcstr ("FOO"), strfcstr ("FOO") };
  struct enum_t en_dup = { .keys = keys_dup, .len = 2 };
  test_assert_int_equal (enum_t_validate (&en_dup, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* empty key */
  struct string keys_empty[2] = { strfcstr ("FOO"), strfcstr ("") };
  struct enum_t en_empty = { .keys = keys_empty, .len = 2 };
  test_assert_int_equal (enum_t_validate (&en_empty, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* empty */
  struct enum_t en_none = { .keys = keys_empty, .len = 0 };
  test_assert_int_equal (enum_t_validate (&en_none, &err), ERR_INTERP);
  err.cause_code = SUCCESS;
}
#endif

i32
enum_t_snprintf (char *str, u32 size, const struct enum_t *st)
{
  DBG_ASSERT (valid_enum_t, st);

  char *out = str;
  u32 avail = size;
  int len = 0;
  int n;

  n = i_snprintf (out, avail, "enum { ");
  if (n < 0)
    {
      return n;
    }
  len += n;
  if (out)
    {
      out += n;
      if ((u32)n < avail)
        {
          avail -= n;
        }
      else
        {
          avail = 0;
        }
    }

  for (u16 i = 0; i < st->len; ++i)
    {
      struct string key = st->keys[i];

      n = i_snprintf (out, avail, "%.*s", key.len, key.data);
      if (n < 0)
        {
          return n;
        }
      len += n;
      if (out)
        {
          out += n;
          if ((u32)n < avail)
            {
              avail -= n;
            }
          else
            {
              avail = 0;
            }
        }

      if ((u16)(i + 1) < st->len)
        {
          n = i_snprintf (out, avail, ", ");
          if (n < 0)
            {
              return n;
            }
          len += n;
          if (out)
            {
              out += n;
              if ((u32)n < avail)
                {
                  avail -= n;
                }
              else
                {
                  avail = 0;
                }
            }
        }
    }

  n = i_snprintf (out, avail, " }");
  if (n < 0)
    {
      return n;
    }
  len += n;

  return len;
}

#ifndef NTEST
TEST (TT_UNIT, enum_t_snprintf)
{
  struct string keys[2] = {
    strfcstr ("FOO"),
    strfcstr ("BARZBI"),
  };

  struct enum_t e = {
    .keys = keys,
    .len = 2,
  };

  char buffer[200];
  const char *expected = "enum { FOO, BARZBI }";
  u32 len = i_strlen (expected);

  int i = enum_t_snprintf (buffer, 200, &e);

  test_assert_int_equal (i, len);
  test_assert_int_equal (i_strncmp (expected, buffer, len), 0);
}
#endif

u32
enum_t_get_serial_size (const struct enum_t *t)
{
  DBG_ASSERT (valid_enum_t, t);
  u32 ret = 0;

  /**
   * LEN (DLEN DATA) (DLEN DATA) (DLEN DATA) ...
   */
  ret += sizeof (u16);
  ret += t->len * sizeof (u16);

  for (u32 i = 0; i < t->len; ++i)
    {
      ret += t->keys[i].len;
    }

  return ret;
}

#ifndef NTEST
TEST (TT_UNIT, enum_t_get_serial_size)
{
  struct enum_t en;
  en.len = 4;
  en.keys = (struct string[]){
    {
        .len = 3,
        .data = "foo",
    },
    {
        .len = 2,
        .data = "fo",
    },
    {
        .len = 4,
        .data = "baro",
    },
    {
        .len = 5,
        .data = "bazbi",
    },
  };

  /* Expected */
  u16 exp = 0;
  exp += sizeof (u16);                     /* Len */
  exp += 4 * sizeof (u16) + 3 + 2 + 4 + 5; /* String keys */

  u32 act = enum_t_get_serial_size (&en);

  test_assert_int_equal (exp, act);
}
#endif

void
enum_t_serialize (struct serializer *persistent, const struct enum_t *src)
{
  /**
   * Program correctness: You have enough room in the serializer
   * to serialize an enum before serializing it
   */
  DBG_ASSERT (valid_enum_t, src);
  bool ret;

  /**
   * LEN (DLEN DATA) (DLEN DATA) (DLEN DATA) ...
   */
  ret = srlizr_write (persistent, (const u8 *)&src->len, sizeof (u16));
  ASSERT (ret);

  for (u32 i = 0; i < src->len; ++i)
    {
      struct string next = src->keys[i];
      ret = srlizr_write (persistent, (const u8 *)&next.len, sizeof (u16));
      ASSERT (ret);

      ret = srlizr_write (persistent, (u8 *)next.data, next.len);
      ASSERT (ret);
    }
}

#ifndef NTEST
TEST (TT_UNIT, enum_t_serialize)
{
  struct enum_t en;
  en.len = 4;
  en.keys = (struct string[]){
    {
        .len = 3,
        .data = "foo",
    },
    {
        .len = 2,
        .data = "fo",
    },
    {
        .len = 4,
        .data = "baro",
    },
    {
        .len = 5,
        .data = "bazbi",
    },
  };

  u8 act[200]; /* Sloppy sizing */
  u8 exp[] = { 0, 0,
               0, 0, 'f', 'o', 'o',
               0, 0, 'f', 'o',
               0, 0, 'b', 'a', 'r', 'o',
               0, 0, 'b', 'a', 'z', 'b', 'i' };
  u16 len = 4;
  u16 l0 = 3;
  u16 l2 = 2;
  u16 l3 = 4;
  u16 l4 = 5;
  i_memcpy (&exp[0], &len, sizeof (u16));
  i_memcpy (&exp[2], &l0, sizeof (u16));
  i_memcpy (&exp[7], &l2, sizeof (u16));
  i_memcpy (&exp[11], &l3, sizeof (u16));
  i_memcpy (&exp[17], &l4, sizeof (u16));

  /* Expected */
  struct serializer s = srlizr_create (act, 200);
  enum_t_serialize (&s, &en);

  test_assert_int_equal (s.dlen, sizeof (exp));
  test_assert_int_equal (i_memcmp (act, exp, sizeof (exp)), 0);
}
#endif

err_t
enum_t_deserialize (struct enum_t *persistent, struct deserializer *src, struct chunk_alloc *a, error *e)
{
  ASSERT (persistent);

  struct enum_t en = { 0 };

  /**
   * LEN
   */
  if (!dsrlizr_read ((u8 *)&en.len, sizeof (u16), src))
    {
      goto early_termination;
    }

  struct string *keys = chunk_calloc (a, en.len, sizeof *keys, e);
  if (keys == NULL)
    {
      return e->cause_code;
    }
  en.keys = keys;

  for (u32 i = 0; i < en.len; ++i)
    {
      /**
       * (KLEN
       */
      u16 klen;
      if (!dsrlizr_read ((u8 *)&klen, sizeof (u16), src))
        {
          goto early_termination;
        }

      en.keys[i] = (struct string){
        .len = (u32)klen,
        .data = chunk_malloc (a, klen, 1, e),
      };

      if (en.keys[i].data == NULL)
        {
          return e->cause_code;
        }

      /**
       * KEY)
       */
      if (!dsrlizr_read ((u8 *)en.keys[i].data, en.keys[i].len, src))
        {
          goto early_termination;
        }
    }

  err_t_wrap (enum_t_validate (&en, e), e);

  *persistent = en;
  return SUCCESS;

early_termination:
  return error_causef (e, ERR_CORRUPT, "Early end of serialized string");
}

#ifndef NTEST
TEST (TT_UNIT, enum_t_deserialize_green_path)
{
  u8 data[] = { 0, 0,
                0, 0, 'f', 'o', 'o',
                0, 0, 'f', 'o',
                0, 0, 'b', 'a', 'r', 'o',
                0, 0, 'b', 'a', 'z', 'b', 'i' };
  u16 len = 4;
  u16 l0 = 3;
  u16 l2 = 2;
  u16 l3 = 4;
  u16 l4 = 5;
  i_memcpy (&data[0], &len, sizeof (u16));
  i_memcpy (&data[2], &l0, sizeof (u16));
  i_memcpy (&data[7], &l2, sizeof (u16));
  i_memcpy (&data[11], &l3, sizeof (u16));
  i_memcpy (&data[17], &l4, sizeof (u16));

  struct chunk_alloc en_temp;
  chunk_alloc_create_default (&en_temp);

  struct deserializer d = dsrlizr_create (data, sizeof (data));

  error e = error_create ();

  struct enum_t eret;
  err_t ret = enum_t_deserialize (&eret, &d, &en_temp, &e);

  test_assert_int_equal (ret, SUCCESS);

  test_assert_int_equal (eret.len, 4);

  test_assert_int_equal (eret.keys[0].len, 3);
  test_assert_int_equal (i_memcmp (eret.keys[0].data, "foo", 3), 0);

  test_assert_int_equal (eret.keys[1].len, 2);
  test_assert_int_equal (i_memcmp (eret.keys[1].data, "fo", 2), 0);

  test_assert_int_equal (eret.keys[2].len, 4);
  test_assert_int_equal (i_memcmp (eret.keys[2].data, "baro", 4), 0);

  test_assert_int_equal (eret.keys[3].len, 5);
  test_assert_int_equal (i_memcmp (eret.keys[3].data, "bazbi", 5), 0);

  chunk_alloc_free_all (&en_temp);
}
#endif

#ifndef NTEST
TEST (TT_UNIT, enum_t_deserialize_red_path)
{
  u8 data[] = { 0, 0,
                0, 0, 'f', 'o', 'o', /* Duplicate */
                0, 0, 'f', 'o', 'o', /* Duplicate */
                0, 0, 'b', 'a', 'r', 'o',
                0, 0, 'b', 'a', 'z', 'b', 'i' };
  u16 len = 4;
  u16 l0 = 3;
  u16 l2 = 3;
  u16 l3 = 4;
  u16 l4 = 5;
  i_memcpy (&data[0], &len, sizeof (u16));
  i_memcpy (&data[2], &l0, sizeof (u16));
  i_memcpy (&data[7], &l2, sizeof (u16));
  i_memcpy (&data[12], &l3, sizeof (u16));
  i_memcpy (&data[18], &l4, sizeof (u16));

  struct enum_t eret;
  struct chunk_alloc temp;
  chunk_alloc_create_default (&temp);
  struct deserializer d = dsrlizr_create (data, sizeof (data));

  error e = error_create ();
  err_t ret = enum_t_deserialize (&eret, &d, &temp, &e);

  test_assert_int_equal (ret, ERR_INTERP); /* Duplicate */

  chunk_alloc_free_all (&temp);
}
#endif

err_t
enum_t_random (struct enum_t *en, struct chunk_alloc *temp, error *e)
{
  ASSERT (en);

  en->len = (u16)randu32r (1, 5);

  en->keys = (struct string *)chunk_malloc (temp, en->len, sizeof (struct string), e);
  if (!en->keys)
    {
      return e->cause_code;
    }

  for (u16 i = 0; i < en->len; ++i)
    {
      err_t_wrap (rand_str (&en->keys[i], temp, 5, 11, e), e);
    }

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, enum_t_random)
{
  error err = error_create ();
  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);

  struct enum_t en;
  test_assert_int_equal (enum_t_random (&en, &arena, &err), SUCCESS);
  /* Generated enum must be valid according to enum_t_validate */
  test_assert_int_equal (enum_t_validate (&en, &err), SUCCESS);

  chunk_alloc_free_all (&arena);
}
#endif

bool
enum_t_equal (const struct enum_t *left, const struct enum_t *right)
{
  DBG_ASSERT (valid_enum_t, left);
  DBG_ASSERT (valid_enum_t, right);

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
    }

  return true;
}

//////////////////////////////
/// BUILDER

DEFINE_DBG_ASSERT (
    struct enum_builder, enum_builder, s,
    {
      ASSERT (s);
    })

void
enb_create (struct enum_builder *dest, struct chunk_alloc *temp, struct chunk_alloc *persistent)
{
  *dest = (struct enum_builder){
    .head = NULL,
    .temp = temp,
    .persistent = persistent,
  };
}

static bool
enb_has_key_been_used (const struct enum_builder *eb, struct string key)
{
  for (struct llnode *it = eb->head; it; it = it->next)
    {
      struct k_llnode *kn = container_of (it, struct k_llnode, link);
      if (string_equal (kn->key, key))
        {
          return true;
        }
    }
  return false;
}

err_t
enb_accept_key (struct enum_builder *eb, struct string key, error *e)
{
  DBG_ASSERT (enum_builder, eb);

  if (key.len == 0)
    {
      return error_causef (
          e, ERR_INTERP,
          "Key length must be > 0");
    }

  if (enb_has_key_been_used (eb, key))
    {
      return error_causef (
          e, ERR_INTERP,
          "Key '%.*s' already used",
          key.len, key.data);
    }

  // Move key data into persistent memory
  key.data = chunk_alloc_move_mem (eb->persistent, key.data, key.len, e);
  if (key.data == NULL)
    {
      return e->cause_code;
    }

  u16 idx = (u16)list_length (eb->head);
  struct llnode *slot = llnode_get_n (eb->head, idx);
  struct k_llnode *node;
  if (slot)
    {
      node = container_of (slot, struct k_llnode, link);
    }
  else
    {
      node = chunk_malloc (eb->temp, 1, sizeof *node, e);
      if (!node)
        {
          return e->cause_code;
        }
      llnode_init (&node->link);
      node->key = (struct string){ 0 };
      if (!eb->head)
        {
          eb->head = &node->link;
        }
      else
        {
          list_append (&eb->head, &node->link);
        }
    }

  node->key = key;
  return SUCCESS;
}

err_t
enb_build (struct enum_t *persistent, struct enum_builder *eb, error *e)
{
  DBG_ASSERT (enum_builder, eb);
  ASSERT (persistent);

  u16 len = (u16)list_length (eb->head);
  if (len == 0)
    {
      return error_causef (e, ERR_INTERP, "no keys to build");
    }

  struct string *keys = chunk_malloc (eb->persistent, len, sizeof *keys, e);
  if (!keys)
    {
      return e->cause_code;
    }

  u16 i = 0;
  for (struct llnode *it = eb->head; it; it = it->next)
    {
      struct k_llnode *kn = container_of (it, struct k_llnode, link);
      keys[i++] = kn->key;
    }

  persistent->len = len;
  persistent->keys = keys;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, enum_builder)
{
  error err = error_create ();
  u8 _temp[2048];
  u8 _persistent[2048];

  /* provide two simple heap tempators for builder + strings */
  struct chunk_alloc persistent;
  chunk_alloc_create_default (&persistent);

  /* 0. freshly‑created builder must be clean */
  struct enum_builder eb;
  enb_create (&eb, &persistent, &persistent);
  test_fail_if (eb.head != NULL);

  /* 1. rejecting empty key */
  test_assert_int_equal (enb_accept_key (&eb, (struct string){ 0 }, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 2. accept first key "A" */
  struct string A = strfcstr ("A");
  test_assert_int_equal (enb_accept_key (&eb, A, &err), SUCCESS);

  /* 3. duplicate key "A" must fail */
  test_assert_int_equal (enb_accept_key (&eb, A, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 4. accept a second key "B" */
  struct string B = strfcstr ("B");
  test_assert_int_equal (enb_accept_key (&eb, B, &err), SUCCESS);

  /* 5. build now that we have two keys */
  struct enum_t en = { 0 };
  test_assert_int_equal (enb_build (&en, &eb, &err), SUCCESS);
  test_assert_int_equal (en.len, 2);
  test_fail_if_null (en.keys);
  test_assert_int_equal (string_equal (en.keys[0], A) || string_equal (en.keys[1], A), true);
  test_assert_int_equal (string_equal (en.keys[0], B) || string_equal (en.keys[1], B), true);

  chunk_alloc_reset_all (&persistent);

  /* 6. build with empty builder must fail */
  struct enum_builder empty;
  enb_create (&empty, &persistent, &persistent);
  struct enum_t en2 = { 0 };
  test_assert_int_equal (enb_build (&en2, &empty, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  chunk_alloc_free_all (&persistent);
}
#endif
