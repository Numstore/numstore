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
 *   Implements compiler.h. Provides the main compiler orchestration logic that
 *   coordinates the scanner and parser to transform input characters into
 *   compiled statements.
 */

#include <numstore/compiler/compiler.h>

#include <numstore/compiler/parser_more.h>
#include <numstore/compiler/scanner.h>
#include <numstore/compiler/tokens.h>
#include <numstore/core/cbuffer.h>

// compiler
// core
struct compiler_s
{
  /* outputs */
  struct cbuffer chars;
  struct cbuffer tokens;
  struct cbuffer statements;

  char _chars[10];
  struct token _tokens[10];
  struct statement_result _statments[10];

  scanner s;
  parser p;
};

DEFINE_DBG_ASSERT (
    struct compiler_s, compiler, s,
    {
      ASSERT (s);
    })

//////////////////////////// Main Functions

struct compiler_s *
compiler_create (error *e)
{
  /* Allocate compiler */
  struct compiler_s *ret = i_malloc (1, sizeof *ret, e);
  if (ret == NULL)
    {
      return NULL;
    }

  *ret = (struct compiler_s){
    .chars = cbuffer_create_from (ret->_chars),
    .tokens = cbuffer_create_from (ret->_tokens),
    .statements = cbuffer_create_from (ret->_statments),
  };

  scanner_init (&ret->s, &ret->chars, &ret->tokens);
  parser_init (&ret->p, &ret->tokens, &ret->statements);

  DBG_ASSERT (compiler, ret);

  return ret;
}

void
compiler_free (struct compiler_s *c)
{
  DBG_ASSERT (compiler, c);
  i_free (c);
}

struct cbuffer *
compiler_get_input (struct compiler_s *c)
{
  DBG_ASSERT (compiler, c);
  return &c->chars;
}

struct cbuffer *
compiler_get_output (struct compiler_s *c)
{
  DBG_ASSERT (compiler, c);
  return &c->statements;
}

void
compiler_execute (struct compiler_s *s)
{
  DBG_ASSERT (compiler, s);

  while (true)
    {
      /**
       * Block on downstream
       */
      if (cbuffer_avail (&s->statements) < sizeof (struct query))
        {
          return;
        }

      /**
       * Block on upstream
       */
      if (cbuffer_len (&s->chars) < sizeof (char))
        {
          return;
        }

      scanner_execute (&s->s);
      parser_execute (&s->p);
    }
}
