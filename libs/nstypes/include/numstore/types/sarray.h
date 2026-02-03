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
 *   Static array type definition with rank, dimensions, and element type, providing
 *   serialization, validation, byte size calculations, and builder pattern.
 */

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/deserializer.h>
#include <numstore/core/error.h>
#include <numstore/core/llist.h>
#include <numstore/core/serializer.h>
#include <numstore/intf/types.h>

////////////////////////////////////////////////////////////
/// MODEL

struct sarray_t
{
  u16 rank;
  u32 *dims;
  struct type *t;
};

err_t sarray_t_validate (const struct sarray_t *t, error *e);
i32 sarray_t_snprintf (char *str, u32 size, const struct sarray_t *p);
u32 sarray_t_byte_size (const struct sarray_t *t);
u32 sarray_t_get_serial_size (const struct sarray_t *t);
void sarray_t_serialize (struct serializer *persistent, const struct sarray_t *src);
err_t sarray_t_deserialize (struct sarray_t *persistent, struct deserializer *src, struct chunk_alloc *a, error *e);
err_t sarray_t_random (struct sarray_t *sa, struct chunk_alloc *temp, u32 depth, error *e);
bool sarray_t_equal (const struct sarray_t *left, const struct sarray_t *right);
