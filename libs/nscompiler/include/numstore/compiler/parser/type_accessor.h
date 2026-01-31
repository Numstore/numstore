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

#include <numstore/compiler/tokens.h>
#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/types/type_accessor.h>

/**
 * EBNF Grammar:
 * =============
 * 
 * accessor        ::= operation*
 * 
 * operation       ::= select | range
 * 
 * select          ::= '.' IDENTIFIER
 * 
 * range           ::= '[' range_expr? ']'
 * 
 * range_expr      ::= index | slice
 * 
 * index           ::= NUMBER
 * 
 * slice           ::= start? ':' end? (':' stride?)?
 * 
 * start           ::= NUMBER
 * end             ::= NUMBER  
 * stride          ::= NUMBER
 * 
 * 
 * Slice Semantics:
 * ----------------
 * [N]          -> index: [N:N+1:1]
 * []           -> empty: [0:DIM:1] (entire dimension)
 * [:]          -> slice: [0:DIM:1] (entire dimension)
 * [::]         -> slice: [0:DIM:1] (entire dimension)
 * [N:]         -> slice: [N:DIM:1] (from N to end)
 * [:N]         -> slice: [0:N:1] (from start to N)
 * [::N]        -> slice: [0:DIM:N] (entire dimension, stride N)
 * [N::M]       -> slice: [N:DIM:M] (from N to end, stride M)
 * [:N:M]       -> slice: [0:N:M] (from start to N, stride M)
 * [N:M]        -> slice: [N:M:1] (from N to M)
 * [N:M:K]      -> slice: [N:M:K] (from N to M, stride K)
 * [::-1]       -> slice: [DIM-1:-1:-1] (reverse, from end to before start)
 * 
 * Note: DIM is the dimension size, determined at compile time when combined with type.
 *       Negative indices/strides are supported (Python-style).
 *       Empty components use sentinel value RANGE_NONE.
 * 
 * 
 * Examples:
 * ---------
 * ""                    -> TA_TAKE
 * ".field"              -> TA_SELECT{name="field"} -> TA_TAKE
 * "[5]"                 -> TA_RANGE{start=5, end=NONE, stride=NONE, is_index=true} -> TA_TAKE
 * "[]"                  -> TA_RANGE{start=NONE, end=NONE, stride=NONE} -> TA_TAKE
 * "[:]"                 -> TA_RANGE{start=NONE, end=NONE, stride=NONE} -> TA_TAKE
 * "[::]"                -> TA_RANGE{start=NONE, end=NONE, stride=NONE} -> TA_TAKE
 * "[0:10]"              -> TA_RANGE{start=0, end=10, stride=NONE} -> TA_TAKE
 * "[0:10:2]"            -> TA_RANGE{start=0, end=10, stride=2} -> TA_TAKE
 * "[1::]"               -> TA_RANGE{start=1, end=NONE, stride=NONE} -> TA_TAKE
 * "[:1:]"               -> TA_RANGE{start=NONE, end=1, stride=NONE} -> TA_TAKE
 * "[::-1]"              -> TA_RANGE{start=NONE, end=NONE, stride=-1} -> TA_TAKE
 * ".x[0:10]"            -> TA_SELECT{name="x"} -> TA_RANGE{0:10:NONE} -> TA_TAKE
 * "[0:5].data[10:20]"   -> TA_RANGE{0:5:NONE} -> TA_SELECT{name="data"} -> TA_RANGE{10:20:NONE} -> TA_TAKE
 * ".matrix[1:10][5:15]" -> TA_SELECT{name="matrix"} -> TA_RANGE{1:10:NONE} -> TA_RANGE{5:15:NONE} -> TA_TAKE
 */

struct type_accessor_parser
{
  struct token *src;
  u32 src_len;
  u32 pos;
  struct type_accessor *result;
  struct chunk_alloc *alloc;
};

/* Parse accessor expression from token stream
 * 
 * Args:
 *   src      - Array of tokens
 *   src_len  - Number of tokens
 *   parser   - Parser state (result and alloc will be set)
 *   e        - Error output
 * 
 * Returns:
 *   SUCCESS on valid parse, error code otherwise
 */
err_t parse_accessor (
    struct token *src,
    u32 src_len,
    struct accessor_parser *parser,
    error *e);

/* Convenience: Parse from string (lexes first, then parses)
 * 
 * Args:
 *   path      - Accessor path string (e.g., ".field[0:10]")
 *   path_len  - Length of path string
 *   result    - Output accessor tree
 *   alloc     - Allocator for nodes
 *   e         - Error output
 */
err_t parse_accessor_str (
    const char *path,
    u32 path_len,
    struct type_accessor **result,
    struct chunk_alloc *alloc,
    error *e);
