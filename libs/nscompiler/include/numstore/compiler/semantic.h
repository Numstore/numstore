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
 *   Semantic analysis functions for type validation. Provides functions to
 *   validate that literal values match expected types, including range checking
 *   and type coercion rules.
 */

// numstore
#include <numstore/compiler/literal.h>
#include <numstore/types/types.h>

// compiler
err_t literal_validate_type (
    bool *islist,                // Literals can be lists
    const struct literal *lit,   // The literal we got
    const struct type *expected, // The type we are checking against
    error *e);

typedef struct
{
  const struct literal *lit;
  const struct type *coerce;
  u32 bsize;   // Cached size of coerce
  u32 written; // Total bytes written <= bsize
} literal_writer;
