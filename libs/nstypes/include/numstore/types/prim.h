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
 *   Primitive type definitions including unsigned/signed integers (8-64 bit),
 *   floating point (16-128 bit), and complex floating point types with size
 *   calculations and serialization support.
 */

// core
#include <numstore/core/deserializer.h>
#include <numstore/core/error.h>
#include <numstore/core/serializer.h>
#include <numstore/core/signatures.h>
#include <numstore/intf/types.h>

enum prim_t
{
  U8 = 0,
  U16 = 1,
  U32 = 2,
  U64 = 3,

  I8 = 4,
  I16 = 5,
  I32 = 6,
  I64 = 7,

  F16 = 8,
  F32 = 9,
  F64 = 10,
  F128 = 11,

  CF32 = 12,
  CF64 = 13,
  CF128 = 14,
  CF256 = 15,

  CI16 = 16,
  CI32 = 17,
  CI64 = 18,
  CI128 = 19,

  CU16 = 20,
  CU32 = 21,
  CU64 = 22,
  CU128 = 23,
};

#define PRIM_INT \
  I8:            \
  case I16:      \
  case I32:      \
  case I64

#define PRIM_UINT \
  U8:             \
  case U16:       \
  case U32:       \
  case U64

#define PRIM_FLOAT \
  F16:             \
  case F32:        \
  case F64:        \
  case F128

#define PRIM_CF \
  CF32:         \
  case CF64:    \
  case CF128:   \
  case CF256

#define PRIM_CI \
  CI16:         \
  case CI32:    \
  case CI64:    \
  case CI128

#define PRIM_CU \
  CU16:         \
  case CU32:    \
  case CU64:    \
  case CU128

HEADER_FUNC bool
prim_is_int (enum prim_t p)
{
  return p >= U8 && p <= I64;
}

HEADER_FUNC bool
prim_is_float (enum prim_t p)
{
  return p >= F16 && p <= F128;
}

HEADER_FUNC bool
prim_is_complex (enum prim_t p)
{
  return p >= CF32 && p <= CU128;
}

#define PRIM_FOR_EACH(func, ...) \
  do                             \
    {                            \
      func (U8, __VA_ARGS__);    \
      func (U8, __VA_ARGS__);    \
      func (U16, __VA_ARGS__);   \
      func (U32, __VA_ARGS__);   \
      func (U64, __VA_ARGS__);   \
      func (I8, __VA_ARGS__);    \
      func (I16, __VA_ARGS__);   \
      func (I32, __VA_ARGS__);   \
      func (I64, __VA_ARGS__);   \
      func (F16, __VA_ARGS__);   \
      func (F32, __VA_ARGS__);   \
      func (F64, __VA_ARGS__);   \
      func (F128, __VA_ARGS__);  \
      func (CF32, __VA_ARGS__);  \
      func (CF64, __VA_ARGS__);  \
      func (CF128, __VA_ARGS__); \
      func (CF256, __VA_ARGS__); \
      func (CI16, __VA_ARGS__);  \
      func (CI32, __VA_ARGS__);  \
      func (CI64, __VA_ARGS__);  \
      func (CI128, __VA_ARGS__); \
      func (CU16, __VA_ARGS__);  \
      func (CU32, __VA_ARGS__);  \
      func (CU64, __VA_ARGS__);  \
      func (CU128, __VA_ARGS__); \
    }                            \
  while (0)

#define PRIM_FOR_EACH_LITERAL(func, ...) \
  do                                     \
    {                                    \
      func ("u8", __VA_ARGS__);          \
      func ("u8", __VA_ARGS__);          \
      func ("u16", __VA_ARGS__);         \
      func ("u32", __VA_ARGS__);         \
      func ("u64", __VA_ARGS__);         \
      func ("i8", __VA_ARGS__);          \
      func ("i16", __VA_ARGS__);         \
      func ("i32", __VA_ARGS__);         \
      func ("i64", __VA_ARGS__);         \
      func ("f16", __VA_ARGS__);         \
      func ("f32", __VA_ARGS__);         \
      func ("f64", __VA_ARGS__);         \
      func ("f128", __VA_ARGS__);        \
      func ("cf32", __VA_ARGS__);        \
      func ("cf64", __VA_ARGS__);        \
      func ("cf128", __VA_ARGS__);       \
      func ("cf256", __VA_ARGS__);       \
      func ("ci16", __VA_ARGS__);        \
      func ("ci32", __VA_ARGS__);        \
      func ("ci64", __VA_ARGS__);        \
      func ("ci128", __VA_ARGS__);       \
      func ("cu16", __VA_ARGS__);        \
      func ("cu32", __VA_ARGS__);        \
      func ("cu64", __VA_ARGS__);        \
      func ("cu128", __VA_ARGS__);       \
    }                                    \
  while (0)

typedef enum
{
  LUB_U64,
  LUB_I64,
  LUB_F128,
  LUB_CF256,
  LUB_CI128,
  LUB_CU128,
} lub_prim_t;

const char *prim_to_str (enum prim_t p);
err_t prim_t_validate (const enum prim_t *t, error *e);
i32 prim_t_snprintf (char *str, u32 size, const enum prim_t *p);
u32 prim_t_byte_size (const enum prim_t *t);
void prim_t_serialize (struct serializer *dest, const enum prim_t *src);
err_t prim_t_deserialize (enum prim_t *dest, struct deserializer *src, error *e);
enum prim_t prim_t_random (void);
enum prim_t strtoprim (const char *text, u32 len);
