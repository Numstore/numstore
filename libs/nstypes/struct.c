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
 *   Implements struct.h. Provides struct type operations including validation,
 *   byte size calculations, serialization, deserialization, and random generation.
 */

#include "numstore/core/chunk_alloc.h"
#include <numstore/types/struct.h>

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/core/random.h>
#include <numstore/core/strings_utils.h>
#include <numstore/intf/stdlib.h>
#include <numstore/test/testing.h>
#include <numstore/types/kvt_builder.h>
#include <numstore/types/types.h>

DEFINE_DBG_ASSERT (struct struct_t, unchecked_struct_t, s,
                   {
                     ASSERT (s);
                     ASSERT (s->keys);
                     ASSERT (s->types);
                   })

static inline err_t
struct_t_type_err (const char *msg, error *e)
{
  return error_causef (e, ERR_INTERP, "Struct: %s", msg);
}

static inline err_t
struct_t_type_deser (const char *msg, error *e)
{
  return error_causef (e, ERR_CORRUPT, "Struct: %s", msg);
}

static err_t
struct_t_validate_shallow (const struct struct_t *s, error *e)
{
  DBG_ASSERT (unchecked_struct_t, s);

  if (s->len == 0)
    {
      return struct_t_type_err ("Keys length must be > 0", e);
    }

  for (u32 i = 0; i < s->len; ++i)
    {
      if (s->keys[i].len == 0)
        {
          return struct_t_type_err ("Key length must be > 0", e);
        }
      ASSERT (s->keys[i].data);
    }

  if (!strings_all_unique (s->keys, s->len))
    {
      return struct_t_type_err ("Duplicate keys", e);
    }

  return SUCCESS;
}

DEFINE_DBG_ASSERT (struct struct_t, valid_struct_t, s,
                   {
                     error e = error_create ();
                     ASSERT (struct_t_validate_shallow (s, &e) == SUCCESS);
                   })

err_t
struct_t_validate (const struct struct_t *s, error *e)
{
  err_t_wrap (struct_t_validate_shallow (s, e), e);
  {
    return false;
  }
  for (u32 i = 0; i < s->len; ++i)
    {
      err_t_wrap (type_validate (&s->types[i], e), e);
      {
        return false;
      }
    }
  return true;
}

i32
struct_t_snprintf (char *str, u32 size, const struct struct_t *st)
{
  DBG_ASSERT (valid_struct_t, st);

  char *out = str;
  u32 avail = size;
  int len = 0;
  int n;

  n = snprintf (out, avail, "struct { ");
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

  for (u32 i = 0; i < st->len; ++i)
    {
      struct string key = st->keys[i];
      n = snprintf (out, avail, "%.*s ", key.len, key.data);
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

      n = type_snprintf (out, avail, &st->types[i]);
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

      if (i + 1 < st->len)
        {
          n = snprintf (out, avail, ", ");
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

  n = snprintf (out, avail, " }");
  if (n < 0)
    {
      return n;
    }
  len += n;

  return len;
}

#ifndef NTEST
TEST (TT_UNIT, struct_t_snprintf)
{
  struct struct_t st;
  st.len = 4;
  st.keys = (struct string[]){
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
  st.types = (struct type[]){
    {
        .type = T_PRIM,
        .p = U32,
    },
    {
        .type = T_PRIM,
        .p = U8,
    },
    {
        .type = T_PRIM,
        .p = U16,
    },
    {
        .type = T_PRIM,
        .p = CF128,
    },
  };

  char buffer[200];
  const char *expected = "struct { foo U32, fo U8, baro U16, bazbi CF128 }";
  u32 len = i_strlen (expected);

  int i = struct_t_snprintf (buffer, 200, &st);

  test_assert_int_equal (i, len);
  test_assert_int_equal (i_strncmp (expected, buffer, len), 0);
}
#endif

u32
struct_t_byte_size (const struct struct_t *t)
{
  DBG_ASSERT (valid_struct_t, t);
  u32 ret = 0;

  /**
   * Each type is layed out contiguously
   */
  for (u32 i = 0; i < t->len; ++i)
    {
      ret += type_byte_size (&t->types[i]);
    }

  return ret;
}

#ifndef NTEST
TEST (TT_UNIT, struct_t_byte_size)
{
  struct struct_t st;
  st.len = 4;
  st.keys = (struct string[]){
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
  st.types = (struct type[]){
    {
        .type = T_PRIM,
        .p = U32,
    },
    {
        .type = T_PRIM,
        .p = U8,
    },
    {
        .type = T_PRIM,
        .p = U16,
    },
    {
        .type = T_PRIM,
        .p = CF128,
    },
  };

  u64 act = struct_t_byte_size (&st);
  u64 exp = (sizeof (u32) + sizeof (u8) + sizeof (u16) + sizeof (cf128));

  test_assert_int_equal (exp, act);
}
#endif

u32
struct_t_get_serial_size (const struct struct_t *t)
{
  DBG_ASSERT (valid_struct_t, t);
  u32 ret = 0;

  /**
   * LEN (KLEN KEY) (TYPE) (KLEN KEY) (TYPE) ....
   */
  ret += sizeof (u16);
  ret += t->len * sizeof (u16);

  for (u32 i = 0; i < t->len; ++i)
    {
      ret += t->keys[i].len;
      ret += type_get_serial_size (&t->types[i]);
    }

  return ret;
}

#ifndef NTEST
TEST (TT_UNIT, struct_t_get_serial_size)
{
  struct struct_t st;
  st.len = 4;
  st.keys = (struct string[]){
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
  st.types = (struct type[]){
    {
        .type = T_PRIM,
        .p = U32,
    },
    {
        .type = T_PRIM,
        .p = U8,
    },
    {
        .type = T_PRIM,
        .p = U16,
    },
    {
        .type = T_PRIM,
        .p = CF128,
    },
  };

  u64 act = struct_t_get_serial_size (&st);
  u64 exp = 2 + 4 * 2 + 3 + 2 + 4 + 5 + 4 * 2;

  test_assert_int_equal (exp, act);
}
#endif

void
struct_t_serialize (struct serializer *dest, const struct struct_t *src)
{
  DBG_ASSERT (valid_struct_t, src);
  bool ret;

  /**
   * LEN (KLEN KEY) (TYPE) (KLEN KEY) (TYPE) ....
   */
  ret = srlizr_write (dest, (const u8 *)&src->len, sizeof (u16));
  ASSERT (ret);

  for (u32 i = 0; i < src->len; ++i)
    {
      /**
       * (KLEN
       */
      struct string next = src->keys[i];
      ret = srlizr_write (dest, (const u8 *)&next.len, sizeof (u16));
      ASSERT (ret);

      /**
       * KEY)
       */
      ret = srlizr_write (dest, (u8 *)next.data, next.len);
      ASSERT (ret);

      /**
       * (TYPE)
       */
      type_serialize (dest, &src->types[i]);
    }
}

#ifndef NTEST
TEST (TT_UNIT, struct_t_serialize)
{
  struct struct_t st;
  st.len = 4;
  st.keys = (struct string[]){
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
  st.types = (struct type[]){
    {
        .type = T_PRIM,
        .p = U32,
    },
    {
        .type = T_PRIM,
        .p = U8,
    },
    {
        .type = T_PRIM,
        .p = U16,
    },
    {
        .type = T_PRIM,
        .p = CF128,
    },
  };

  u8 act[200]; /* Sloppy sizing */
  u8 exp[] = { 0, 0,
               0, 0, 'f', 'o', 'o', (u8)T_PRIM, (u8)U32,
               0, 0, 'f', 'o', (u8)T_PRIM, (u8)U8,
               0, 0, 'b', 'a', 'r', 'o', (u8)T_PRIM, (u8)U16,
               0, 0, 'b', 'a', 'z', 'b', 'i', (u8)T_PRIM, (u8)CF128 };
  u16 len = 4;
  u16 l0 = 3;
  u16 l2 = 2;
  u16 l3 = 4;
  u16 l4 = 5;
  i_memcpy (&exp[0], &len, sizeof (u16));
  i_memcpy (&exp[2], &l0, sizeof (u16));
  i_memcpy (&exp[9], &l2, sizeof (u16));
  i_memcpy (&exp[15], &l3, sizeof (u16));
  i_memcpy (&exp[23], &l4, sizeof (u16));

  /* Expected */
  struct serializer s = srlizr_create (act, 200);
  struct_t_serialize (&s, &st);

  test_assert_int_equal (s.dlen, sizeof (exp));
  test_assert_int_equal (i_memcmp (act, exp, sizeof (exp)), 0);
}
#endif

err_t
struct_t_deserialize (
    struct struct_t *dest,
    struct deserializer *src,
    struct chunk_alloc *a,
    error *e)
{
  ASSERT (dest);

  struct chunk_alloc temp;
  chunk_alloc_create_default (&temp);

  struct kvt_builder unb;
  kvb_create (&unb, &temp, a);

  /**
   * LEN
   */
  u16 len;
  if (!dsrlizr_read ((u8 *)&len, sizeof (u16), src))
    {
      goto early_termination;
    }

  for (u32 i = 0; i < len; ++i)
    {
      /* Read the string key length */
      u16 klen;
      if (!dsrlizr_read ((u8 *)&klen, sizeof (u16), src))
        {
          goto early_termination;
        }

      struct string key = {
        .len = klen,
        .data = chunk_malloc (a, key.len, 1, e),
      };
      /* Read the string data */
      if (key.data == NULL)
        {
          goto theend;
        }
      if (!dsrlizr_read ((u8 *)key.data, key.len, src))
        {
          goto early_termination;
        }

      /* Deserialize sub type */
      struct type t;
      err_t_wrap_goto (type_deserialize (&t, src, a, e), theend, e);

      err_t_wrap_goto (kvb_accept_key (&unb, key, e), theend, e);
      err_t_wrap_goto (kvb_accept_type (&unb, t, e), theend, e);
    }

  err_t_wrap_goto (kvb_struct_t_build (dest, &unb, e), theend, e);

theend:
  chunk_alloc_free_all (&temp);
  return e->cause_code;

early_termination:
  chunk_alloc_free_all (&temp);
  return struct_t_type_deser ("Early end of serialized string", e);
}

#ifndef NTEST
TEST (TT_UNIT, struct_t_deserialize_green_path)
{
  u8 data[] = { 0, 0,
                0, 0, 'f', 'o', 'o', (u8)T_PRIM, (u8)U32,
                0, 0, 'f', 'o', (u8)T_PRIM, (u8)U8,
                0, 0, 'b', 'a', 'r', 'o', (u8)T_PRIM, (u8)U16,
                0, 0, 'b', 'a', 'z', 'b', 'i', (u8)T_PRIM, (u8)CF128 };
  u16 len = 4;
  u16 l0 = 3;
  u16 l2 = 2;
  u16 l3 = 4;
  u16 l4 = 5;
  i_memcpy (&data[0], &len, sizeof (u16));
  i_memcpy (&data[2], &l0, sizeof (u16));
  i_memcpy (&data[9], &l2, sizeof (u16));
  i_memcpy (&data[15], &l3, sizeof (u16));
  i_memcpy (&data[23], &l4, sizeof (u16));

  struct chunk_alloc st_alloc;
  chunk_alloc_create_default (&st_alloc);

  struct deserializer d = dsrlizr_create (data, sizeof (data));

  error e = error_create ();

  struct struct_t eret;
  err_t ret = struct_t_deserialize (&eret, &d, &st_alloc, &e);

  test_assert_int_equal (ret, SUCCESS);

  test_assert_int_equal (eret.len, 4);

  test_assert_int_equal (eret.keys[0].len, 3);
  test_assert_int_equal (i_memcmp (eret.keys[0].data, "foo", 3), 0);
  test_assert_int_equal (eret.types[0].type, T_PRIM);
  test_assert_int_equal (eret.types[0].p, U32);

  test_assert_int_equal (eret.keys[1].len, 2);
  test_assert_int_equal (i_memcmp (eret.keys[1].data, "fo", 2), 0);
  test_assert_int_equal (eret.types[1].type, T_PRIM);
  test_assert_int_equal (eret.types[1].p, U8);

  test_assert_int_equal (eret.keys[2].len, 4);
  test_assert_int_equal (i_memcmp (eret.keys[2].data, "baro", 4), 0);
  test_assert_int_equal (eret.types[2].type, T_PRIM);
  test_assert_int_equal (eret.types[2].p, U16);

  test_assert_int_equal (eret.keys[3].len, 5);
  test_assert_int_equal (i_memcmp (eret.keys[3].data, "bazbi", 5), 0);
  test_assert_int_equal (eret.types[3].type, T_PRIM);
  test_assert_int_equal (eret.types[3].p, CF128);
}
#endif

#ifndef NTEST
TEST (TT_UNIT, struct_t_deserialize_red_path)
{
  u8 data[] = { 0, 0,                                     /* Total length (4) */
                0, 0, 'f', 'o', 'o', (u8)T_PRIM, (u8)U32, /* STRLEN STRING (sub) TYPE */
                0, 0, 'f', 'o', 'o', (u8)T_PRIM, (u8)U8,
                0, 0, 'b', 'a', 'r', 'o', (u8)T_PRIM, (u8)U16,
                0, 0, 'b', 'a', 'z', 'b', 'i', (u8)T_PRIM, (u8)CF128 };
  u16 len = 4;
  u16 l0 = 3;
  u16 l2 = 3;
  u16 l3 = 4;
  u16 l4 = 5;
  i_memcpy (&data[0], &len, sizeof (u16));
  i_memcpy (&data[2], &l0, sizeof (u16));
  i_memcpy (&data[9], &l2, sizeof (u16));
  i_memcpy (&data[16], &l3, sizeof (u16));
  i_memcpy (&data[24], &l4, sizeof (u16));

  struct struct_t sret;
  struct chunk_alloc alloc;
  chunk_alloc_create_default (&alloc);
  struct deserializer d = dsrlizr_create (data, sizeof (data));

  error e = error_create ();
  err_t ret = struct_t_deserialize (&sret, &d, &alloc, &e);

  test_assert_int_equal (ret, ERR_INTERP); /* Duplicate */
}
#endif

err_t
struct_t_random (struct struct_t *st, struct chunk_alloc *alloc, u32 depth, error *e)
{
  ASSERT (st);

  st->len = (u16)randu32r (1, 5);

  st->keys = (struct string *)chunk_malloc (alloc, st->len, sizeof (struct string), e);
  if (!st->keys)
    {
      return e->cause_code;
    }

  st->types = (struct type *)chunk_malloc (alloc, st->len, sizeof (struct type), e);
  if (!st->types)
    {
      return e->cause_code;
    }

  for (u16 i = 0; i < st->len; ++i)
    {
      err_t_wrap (rand_str (&st->keys[i], alloc, 5, 11, e), e);
      err_t_wrap (type_random (&st->types[i], alloc, depth - 1, e), e);
    }

  return SUCCESS;
}

bool
struct_t_equal (const struct struct_t *left, const struct struct_t *right)
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
      if (!type_equal (&left->types[i], &right->types[i]))
        {
          return false;
        }
    }

  return true;
}
