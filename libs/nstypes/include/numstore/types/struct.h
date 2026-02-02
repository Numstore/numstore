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
 *   Struct type definition with named fields and nested types, providing serialization,
 *   validation, byte size calculations, and builder pattern for construction.
 */

// core
#include <numstore/core/chunk_alloc.h>
#include <numstore/core/deserializer.h>
#include <numstore/core/error.h>
#include <numstore/core/serializer.h>
#include <numstore/core/string.h>

////////////////////////////////////////////////////////////
/// MODEL

struct struct_t
{
  u16 len;
  struct string *keys;
  struct type *types;
};

err_t struct_t_validate (const struct struct_t *t, error *e);
i32 struct_t_snprintf (char *str, u32 size, const struct struct_t *st);
u32 struct_t_byte_size (const struct struct_t *t);
u32 struct_t_get_serial_size (const struct struct_t *t);
void struct_t_serialize (struct serializer *dest, const struct struct_t *src);
err_t struct_t_deserialize (struct struct_t *dest, struct deserializer *src, struct chunk_alloc *a, error *e);
err_t struct_t_random (struct struct_t *st, struct chunk_alloc *alloc, u32 depth, error *e);
bool struct_t_equal (const struct struct_t *left, const struct struct_t *right);
struct type *struct_t_resolve_key (t_size *offset, struct struct_t *t, struct string key, error *e);
