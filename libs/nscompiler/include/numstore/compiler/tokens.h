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
 *   Token types and structures for the scanner. Defines all token types
 *   (operators, keywords, literals, identifiers) with associated data,
 *   token comparison functions, and string conversion utilities.
 */

// compiler
#include <numstore/compiler/statement.h>

enum token_t
{
  // Arithmetic Operators
  TT_PLUS = 1, // +
  TT_MINUS,    // -
  TT_SLASH,    // /
  TT_STAR,     // *

  // Logical Operators
  TT_BANG,          // !
  TT_BANG_EQUAL,    // !=
  TT_EQUAL_EQUAL,   // ==
  TT_GREATER,       // >
  TT_GREATER_EQUAL, // >=
  TT_LESS,          // <
  TT_LESS_EQUAL,    // <=

  // Fancy Operators
  TT_NOT,                 // ~
  TT_CARET,               // ^
  TT_PERCENT,             // %
  TT_PIPE,                // |
  TT_PIPE_PIPE,           // ||
  TT_AMPERSAND,           // &
  TT_AMPERSAND_AMPERSAND, // &&

  // Other One char tokens
  TT_SEMICOLON,     // ;
  TT_COLON,         // :
  TT_LEFT_BRACKET,  // [
  TT_RIGHT_BRACKET, // ]
  TT_LEFT_BRACE,    // {
  TT_RIGHT_BRACE,   // }
  TT_LEFT_PAREN,    // (
  TT_RIGHT_PAREN,   // )
  TT_COMMA,         // ,

  // Other
  TT_STRING,     // "[A-Za-z0-9]*"$
  TT_IDENTIFIER, // ^[A-Za-z][A-Za-z0-9]*$

  // Tokens that start with a number or +/-
  TT_INTEGER, // integer
  TT_FLOAT,   // float

  // Literal Operations
  TT_CREATE, // create
  TT_DELETE, // delete
  TT_INSERT, // insert

  // Type literals
  TT_STRUCT, // struct
  TT_UNION,  // union
  TT_ENUM,   // enum
  TT_PRIM,   // prim

  // Bools
  TT_TRUE,  // true
  TT_FALSE, // false

  // Special Error token on failed scan
  TT_ERROR,
};

#define case_OPCODE \
  TT_CREATE:        \
  case TT_DELETE:   \
  case TT_INSERT

HEADER_FUNC bool
tt_is_opcode (enum token_t ttype)
{
  switch (ttype)
    {
    case case_OPCODE:
      {
        return true;
      }
    default:
      {
        return false;
      }
    }
}

struct token
{
  union
  {
    struct string str;      /* STRING or IDENTIFIER */
    i32 integer;            /* TT_INTEGER */
    f32 floating;           /* TT_FLOAT */
    enum prim_t prim;       /* TT_PRIM */
    struct statement *stmt; /* The First TT_OP_CODE */
    error e;                /* TT_ERROR */
  };
  enum token_t type;
};

/* Shorthands */
#define quick_tok(_type) \
  (struct token) { .type = _type }

#define tt_integer(val) \
  (struct token) { .type = TT_INTEGER, .integer = val }

#define tt_float(val) \
  (struct token) { .type = TT_FLOAT, .floating = val }

#define tt_ident(val) \
  (struct token) { .type = TT_IDENTIFIER, .str = val }

#define tt_string(val) \
  (struct token) { .type = TT_STRING, .str = val }

#define tt_prim(val) \
  (struct token) { .type = TT_PRIM, .prim = val }

#define tt_opcode(op, _s) \
  (struct token) { .type = op, .stmt = _s }

#define tt_err(_e) \
  (struct token) { .type = TT_ERROR, .e = _e }

#define MAX_TOK_T_LEN 16

bool token_equal (const struct token *left, const struct token *right);

const char *tt_tostr (enum token_t t);
