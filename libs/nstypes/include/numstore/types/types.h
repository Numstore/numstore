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

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/deserializer.h>
#include <numstore/core/error.h>
#include <numstore/core/llist.h>
#include <numstore/core/serializer.h>
#include <numstore/core/string.h>
#include <numstore/types/prim.h>

/////////////////////////////////
/// Type

struct type
{
  enum type_t
  {
    T_PRIM = 0,
    T_STRUCT = 1,
    T_UNION = 2,
    T_ENUM = 3,
    T_SARRAY = 4,
  } type;

  union
  {
    enum prim_t p;

    struct struct_t
    {
      u16 len;
      struct string *keys;
      struct type *types;
    } st;

    struct union_t
    {
      u16 len;
      struct string *keys;
      struct type *types;
    } un;

    struct enum_t
    {
      u16 len;
      struct string *keys;
    } en;

    struct sarray_t
    {
      u16 rank;
      u32 *dims;
      struct type *t;
    } sa;
  };
};

err_t type_validate (const struct type *t, error *e);
i32 type_snprintf (char *str, u32 size, struct type *t);
u32 type_byte_size (const struct type *t);
u32 type_get_serial_size (const struct type *t);
void type_serialize (struct serializer *dest, const struct type *src);
err_t type_deserialize (struct type *dest, struct deserializer *src, struct chunk_alloc *alloc, error *e);
err_t type_random (struct type *dest, struct chunk_alloc *alloc, u32 depth, error *e);
bool type_equal (const struct type *left, const struct type *right);
err_t i_log_type (struct type t, error *e);

///////////////////////////////////////
/// KVT List

struct kvt_list
{
  u16 len;
  struct string *keys;
  struct type *types;
};

struct kv_llnode
{
  struct string key;
  struct type value;
  struct llnode link;
};

struct kvt_list_builder
{
  struct llnode *head;

  u16 klen;
  u16 tlen;

  struct chunk_alloc *temp;
  struct chunk_alloc *persistent;
};

void kvlb_create (
    struct kvt_list_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent);

err_t kvlb_accept_key (struct kvt_list_builder *ub, struct string key, error *e);
err_t kvlb_accept_type (struct kvt_list_builder *eb, struct type t, error *e);

err_t kvlb_build (struct kvt_list *dest, struct kvt_list_builder *eb, error *e);

/////////////////////////////////
/// Struct

err_t struct_t_create (struct struct_t *dest, struct kvt_list list, struct chunk_alloc *dalloc, error *e);
err_t struct_t_validate (const struct struct_t *t, error *e);
i32 struct_t_snprintf (char *str, u32 size, const struct struct_t *st);
u32 struct_t_byte_size (const struct struct_t *t);
u32 struct_t_get_serial_size (const struct struct_t *t);
void struct_t_serialize (struct serializer *dest, const struct struct_t *src);
err_t struct_t_deserialize (struct struct_t *dest, struct deserializer *src, struct chunk_alloc *a, error *e);
err_t struct_t_random (struct struct_t *st, struct chunk_alloc *alloc, u32 depth, error *e);
bool struct_t_equal (const struct struct_t *left, const struct struct_t *right);
struct type *struct_t_resolve_key (t_size *offset, struct struct_t *t, struct string key, error *e);

/////////////////////////////////
/// Prim

const char *prim_to_str (enum prim_t p);
err_t prim_t_validate (const enum prim_t *t, error *e);
i32 prim_t_snprintf (char *str, u32 size, const enum prim_t *p);
u32 prim_t_byte_size (const enum prim_t *t);
void prim_t_serialize (struct serializer *dest, const enum prim_t *src);
err_t prim_t_deserialize (enum prim_t *dest, struct deserializer *src, error *e);
enum prim_t prim_t_random (void);
enum prim_t strtoprim (const char *text, u32 len);

/////////////////////////////////
/// Union

err_t union_t_create (struct union_t *dest, struct kvt_list list, struct chunk_alloc *dalloc, error *e);
err_t union_t_validate (const struct union_t *t, error *e);
i32 union_t_snprintf (char *str, u32 size, const struct union_t *p);
u32 union_t_byte_size (const struct union_t *t);
u32 union_t_get_serial_size (const struct union_t *t);
void union_t_serialize (struct serializer *dest, const struct union_t *src);
err_t union_t_deserialize (struct union_t *dest, struct deserializer *src, struct chunk_alloc *a, error *e);
err_t union_t_random (struct union_t *un, struct chunk_alloc *alloc, u32 depth, error *e);
bool union_t_equal (const struct union_t *left, const struct union_t *right);
struct type *union_t_resolve_key (struct union_t *t, struct string key, error *e);

/////////////////////////////////
/// Enum

err_t enum_t_validate (const struct enum_t *t, error *e);
i32 enum_t_snprintf (char *str, u32 size, const struct enum_t *st);
#define enum_t_byte_size(e) sizeof (u8)
u32 enum_t_get_serial_size (const struct enum_t *t);
void enum_t_serialize (struct serializer *persistent, const struct enum_t *src);
err_t enum_t_deserialize (struct enum_t *persistent, struct deserializer *src, struct chunk_alloc *a, error *e);
err_t enum_t_random (struct enum_t *en, struct chunk_alloc *temp, error *e);
bool enum_t_equal (const struct enum_t *left, const struct enum_t *right);

struct k_llnode
{
  struct string key;
  struct llnode link;
};

struct enum_builder
{
  struct llnode *head;
  struct chunk_alloc *temp;
  struct chunk_alloc *persistent;
};

void enb_create (
    struct enum_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent);

err_t enb_accept_key (struct enum_builder *eb, struct string key, error *e);
err_t enb_build (struct enum_t *persistent, struct enum_builder *eb, error *e);

////////////////////////////////////////////////////////////
/// SARRAY

err_t sarray_t_validate (const struct sarray_t *t, error *e);
i32 sarray_t_snprintf (char *str, u32 size, const struct sarray_t *p);
u32 sarray_t_byte_size (const struct sarray_t *t);
u32 sarray_t_get_serial_size (const struct sarray_t *t);
void sarray_t_serialize (struct serializer *persistent, const struct sarray_t *src);
err_t sarray_t_deserialize (struct sarray_t *persistent, struct deserializer *src, struct chunk_alloc *a, error *e);
err_t sarray_t_random (struct sarray_t *sa, struct chunk_alloc *temp, u32 depth, error *e);
bool sarray_t_equal (const struct sarray_t *left, const struct sarray_t *right);

struct dim_llnode
{
  u32 dim;
  struct llnode link;
};

struct sarray_builder
{
  struct llnode *head;
  struct type *type;

  struct chunk_alloc *temp;
  struct chunk_alloc *persistent;
};

void sab_create (struct sarray_builder *dest, struct chunk_alloc *temp, struct chunk_alloc *persistent);
err_t sab_accept_dim (struct sarray_builder *eb, i32 dim, error *e);
err_t sab_accept_type (struct sarray_builder *eb, struct type type, error *e);
err_t sab_build (struct sarray_t *persistent, struct sarray_builder *eb, error *e);
