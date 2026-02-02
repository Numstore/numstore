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
 *   Parser for type accessor expressions that extract data from typed structures.
 *   Supports field selection, array indexing, and slicing operations.
 */

#include <numstore/compiler/parser/parser.h>
#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/types/type_accessor.h>

/**
 * type_accessor   ::= accessor_part ('.' accessor_part)*
 *
 * accessor_part   ::= IDENT stride*
 */
struct type_accessor_parser
{
  struct parser base;

  struct string vname_dest;
  struct type_accessor dest;

  struct chunk_alloc temp;
  struct chunk_alloc *persistent;
};

err_t parse_type_accessor (
    struct token *src,
    u32 src_len,
    struct type_accessor_parser *parser,
    error *e);
