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
 *   Scratch/example program demonstrating parser functionality.
 */

// core
#include <numstore/core/cbuffer.h>
#include <numstore/core/error.h>

#include <compiler/compiler.h>
#include <compiler/parser_more.h>
#include <compiler/queries/query.h>
#include <compiler/statement.h>
#include <compiler/tokens.h>

// compiler
int
main (void)
{
  struct token _input[4096];
  struct statement_result _stmts[10];

  struct cbuffer input = cbuffer_create_from (_input);
  struct cbuffer output = cbuffer_create_from (_stmts);

  error e = error_create ();
  parser p;
  parser_init (&p, &input, &output);

  struct statement *s = statement_create (&e);

  struct token tokens[] = {
    tt_opcode (TT_CREATE, s),
    tt_ident (strfcstr ("a")),

    /* struct { */
    quick_tok (TT_STRUCT),
    quick_tok (TT_LEFT_BRACE),

    /* foo U32, */
    tt_ident (strfcstr ("foo")),
    tt_prim (U32),
    quick_tok (TT_COMMA),

    /* bar enum { bar, biz, buz, }, // notice trailing comma */
    tt_ident (strfcstr ("bar")),
    quick_tok (TT_ENUM),
    quick_tok (TT_LEFT_BRACE),
    tt_ident (strfcstr ("bar")),
    quick_tok (TT_COMMA),
    tt_ident (strfcstr ("biz")),
    quick_tok (TT_COMMA),
    tt_ident (strfcstr ("buz")),
    quick_tok (TT_RIGHT_BRACE),
    quick_tok (TT_COMMA),

    /* biz struct { a U32, b union { c i32, d u64 } }, */
    tt_ident (strfcstr ("biz")),
    quick_tok (TT_STRUCT),
    quick_tok (TT_LEFT_BRACE),
    tt_ident (strfcstr ("a")),
    tt_prim (U32),
    quick_tok (TT_COMMA),
    tt_ident (strfcstr ("b")),
    quick_tok (TT_UNION),
    quick_tok (TT_LEFT_BRACE),
    tt_ident (strfcstr ("c")),
    tt_prim (I32),
    quick_tok (TT_COMMA),
    tt_ident (strfcstr ("d")),
    tt_prim (U64),
    quick_tok (TT_RIGHT_BRACE),
    quick_tok (TT_RIGHT_BRACE),
    quick_tok (TT_COMMA),

    /* baz [10][20][30] struct { i i32 } */
    tt_ident (strfcstr ("baz")),
    quick_tok (TT_LEFT_BRACKET),
    tt_integer (10),
    quick_tok (TT_RIGHT_BRACKET),
    quick_tok (TT_LEFT_BRACKET),
    tt_integer (20),
    quick_tok (TT_RIGHT_BRACKET),
    quick_tok (TT_LEFT_BRACKET),
    tt_integer (30),
    quick_tok (TT_RIGHT_BRACKET),
    quick_tok (TT_STRUCT),
    quick_tok (TT_LEFT_BRACE),
    tt_ident (strfcstr ("i")),
    tt_prim (I32),
    quick_tok (TT_RIGHT_BRACE),

    /* }; */
    quick_tok (TT_RIGHT_BRACE),
    quick_tok (TT_SEMICOLON),
  };

  for (u32 i = 0; i < arrlen (tokens); ++i)
    {
      cbuffer_write (&tokens[i], sizeof (struct token), 1, &input);

      parser_execute (&p);

      struct statement_result res;
      if (cbuffer_read (&res, sizeof (res), 1, &output))
        {
          err_t_wrap (res.e.cause_code, &res.e);
          i_log_query (&res.stmt->q);
        }
    }

  return 0;
}
