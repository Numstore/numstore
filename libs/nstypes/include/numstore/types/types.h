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
 *   Main type system header defining the unified type structure that represents all
 *   data types in NumStore (primitives, structs, unions, enums, static arrays) with
 *   serialization, validation, and utility functions.
 */

// core
#include <numstore/core/deserializer.h>
#include <numstore/core/error.h>
#include <numstore/core/lalloc.h>
#include <numstore/core/serializer.h>
#include <numstore/types/enum.h>
#include <numstore/types/prim.h>
#include <numstore/types/sarray.h>
#include <numstore/types/struct.h>
#include <numstore/types/union.h>

enum type_t
{
  T_PRIM = 0,
  T_STRUCT = 1,
  T_UNION = 2,
  T_ENUM = 3,
  T_SARRAY = 4,
};

struct type
{
  union
  {
    enum prim_t p;
    struct struct_t st;
    struct union_t un;
    struct enum_t en;
    struct sarray_t sa;
  };

  enum type_t type;
};

err_t type_validate (const struct type *t, error *e);
i32 type_snprintf (char *str, u32 size, struct type *t);
u32 type_byte_size (const struct type *t);
u32 type_get_serial_size (const struct type *t);
void type_serialize (struct serializer *dest, const struct type *src);
err_t type_deserialize (struct type *dest, struct deserializer *src, struct lalloc *alloc, error *e);
err_t type_random (struct type *dest, struct lalloc *alloc, u32 depth, error *e);
bool type_equal (const struct type *left, const struct type *right);
err_t i_log_type (struct type t, error *e);
