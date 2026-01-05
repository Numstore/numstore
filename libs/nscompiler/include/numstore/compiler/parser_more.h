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
 *   Parser structures and state management. Defines parser result types,
 *   parser state including lemon parser integration, and functions for
 *   parsing token streams into statements.
 */

// compiler
#include <numstore/compiler/statement.h>
#include <numstore/compiler/tokens.h>

typedef struct
{
  void *yyp;                // The lemon parser
  struct lalloc *work;      // Where to allocate work
  u32 tnum;                 // Token number in the current statement
  error *e;                 // Return status
  struct statement *result; // Resulting statement
  bool done;                // Done parsing this query
} parser_result;

err_t parser_result_create (parser_result *ret, struct lalloc *work, error *e);
struct statement *parser_result_consume (parser_result *p);
err_t parser_result_parse (parser_result *res, struct token tok, error *e);

typedef struct
{
  struct cbuffer *input;     // tokens input
  struct cbuffer *output;    // statement_result output
  parser_result parser;      // One for each statement
  error e;                   // parser related errors passed forward
  struct lalloc parser_work; // Work for parser - to be copied to query space
  u8 _parser_work[2048];
  bool in_err;

#ifndef NDEBUG
  bool expect_opcode;
#endif
} parser;

void parser_init (parser *dest, struct cbuffer *input, struct cbuffer *output);
void parser_execute (parser *p);
