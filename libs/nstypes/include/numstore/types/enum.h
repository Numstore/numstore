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
 *   Enumeration type definition with named keys, providing serialization, validation,
 *   and builder pattern for constructing enumeration types.
 */

#include <numstore/core/deserializer.h>
#include <numstore/core/error.h>
#include <numstore/core/lalloc.h>
#include <numstore/core/llist.h>
#include <numstore/core/serializer.h>
#include <numstore/core/string.h>

////////////////////////////////////////////////////////////
/// MODEL

struct enum_t
{
  u16 len;
  struct string *keys;
};

err_t enum_t_validate (const struct enum_t *t, error *e);
i32 enum_t_snprintf (char *str, u32 size, const struct enum_t *st);
#define enum_t_byte_size(e) sizeof (u8)
u32 enum_t_get_serial_size (const struct enum_t *t);
void enum_t_serialize (struct serializer *dest, const struct enum_t *src);
err_t enum_t_deserialize (struct enum_t *dest, struct deserializer *src, struct lalloc *a, error *e);
err_t enum_t_random (struct enum_t *en, struct lalloc *alloc, error *e);
bool enum_t_equal (const struct enum_t *left, const struct enum_t *right);

////////////////////////////////////////////////////////////
/// BUILDER

struct k_llnode
{
  struct string key;
  struct llnode link;
};

struct enum_builder
{
  struct llnode *head;
  struct lalloc *alloc;
  struct lalloc *dest;
};

struct enum_builder enb_create (struct lalloc *alloc, struct lalloc *dest);
err_t enb_accept_key (struct enum_builder *eb, const struct string key, error *e);
err_t enb_build (struct enum_t *dest, struct enum_builder *eb, error *e);
