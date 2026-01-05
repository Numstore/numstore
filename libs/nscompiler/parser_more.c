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
 *   Implements parser_more.h. Provides parser logic that integrates with
 *   lemon-generated parsers to transform token streams into statement
 *   structures with error handling and state management.
 */

#include <numstore/compiler/parser_more.h>

#include <numstore/compiler/compiler.h>
#include <numstore/core/assert.h>
#include <numstore/core/cbuffer.h>
#include <numstore/intf/logging.h>

#include <stdlib.h>

// TODO: Support both shell_parser and net_parser
// For now, using shell_parser as the default
extern void *shell_parserAlloc (void *(*mallocProc) (size_t));

extern void shell_parserFree (
    void *p,
    void (*freeProc) (void *));

extern void shell_parser (
    void *yyp,
    int yymajor,
    struct token yyminor,
    parser_result *ctxt);

err_t
parser_result_create (parser_result *ret, struct lalloc *work, error *e)
{
  ASSERT (ret);

  void *yyp = shell_parserAlloc (malloc);
  if (yyp == NULL)
    {
      return error_causef (e, ERR_NOMEM, "Failed to allocate lemon parser");
    }

  *ret = (parser_result){
    .yyp = yyp,
    .work = work,
    .tnum = 0,
    .e = NULL,
    .result = NULL,
    .done = false,
  };

  return SUCCESS;
}

static inline void
parser_result_reset (parser_result *p)
{
  if (p->yyp)
    {
      shell_parserFree (p->yyp, free);
      p->yyp = NULL;
    }

  lalloc_reset (p->work);
  p->tnum = 0;
  p->e = NULL;
  p->done = false;
}

static inline void
parser_result_invalidate (parser_result *p)
{
  if (p->result)
    {
      statement_free (p->result);
      p->result = NULL;
    }

  parser_result_reset (p);
}

struct statement *
parser_result_consume (parser_result *p)
{
  ASSERT (p->done);
  ASSERT (p->yyp);

  struct statement *ret = p->result;
  ASSERT (ret);

  parser_result_reset (p);

  return ret;
}

err_t
parser_result_parse (parser_result *res, struct token tok, error *e)
{
  res->e = e;
  shell_parser (res->yyp, tok.type, tok, res);
  err_t ret = res->e->cause_code;

  if (!ret)
    {
      res->tnum++;
    }

  return ret;
}

////////////////////////////////////////////////
// Parser

DEFINE_DBG_ASSERT (
    parser, parser, s,
    {
      ASSERT (s);
    })

DEFINE_DBG_ASSERT (
    parser, parser_steady_state, s,
    {
      DBG_ASSERT (parser, s);
    })

DEFINE_DBG_ASSERT (
    parser, parser_right_after_error, s,
    {
      DBG_ASSERT (parser, s);
      ASSERT (s->parser.result == NULL);
    })

#define parser_err_t_wrap(expr, c)  \
  do                                \
    {                               \
      err_t __ret = (err_t)expr;    \
      if (__ret < SUCCESS)          \
        {                           \
          parser_process_error (c); \
          return __ret;             \
        }                           \
    }                               \
  while (0)

#define parser_err_t_continue(expr, c) \
  do                                   \
    {                                  \
      err_t __ret = (err_t)expr;       \
      if (__ret < SUCCESS)             \
        {                              \
          parser_process_error (c);    \
        }                              \
    }                                  \
  while (0)

static inline void
parser_write_result (parser *s, struct statement *stmt)
{
  ASSERT (stmt);
  struct statement_result result = {
    .stmt = stmt,
    .ok = true,
  };
  cbuffer_write_expect (&result, sizeof (result), 1, s->output);
}

/* Process an error */
static inline void
parser_process_error (parser *s)
{
  if (!s->in_err)
    {
      DBG_ASSERT (parser_right_after_error, s);
      s->in_err = true;

      /* Invalidate the parser (and free the statement) */
      parser_result_invalidate (&s->parser);

      struct statement_result result = {
        .e = s->e,
        .ok = false,
      };

      cbuffer_write_expect (&result, sizeof result, 1, s->output);

      /* Reset error */
      s->e = error_create ();
    }

  DBG_ASSERT (parser_steady_state, s);
}

static inline err_t
execute_normal (parser *s)
{
  DBG_ASSERT (parser_steady_state, s);
  ASSERT (!s->in_err);

  struct token t;
  while (cbuffer_read (&t, sizeof t, 1, s->input))
    {
      i_log_trace ("Parser (Normal State) recieved token: %s\n", tt_tostr (t.type));

      switch (t.type)
        {
        /* If we encounter an OPCODE */
        case case_OPCODE:
          {

#ifndef NDEBUG
            if (s->expect_opcode)
              {
                ASSERT (t.stmt != NULL);
                ASSERT (s->parser.yyp == NULL);
                s->expect_opcode = false;
              }
#endif

            /* And statement is non null */
            if (t.stmt != NULL)
              {
                TODO ("Prove that this is true");
                ASSERT (s->parser.yyp == NULL);

                /* Start a new lemon parser */
                parser_err_t_wrap (parser_result_create (&s->parser, &s->parser_work, &s->e), s);
              }
            break;
          }
        /* Forward scanner error */
        case TT_ERROR:
          {

            /* Transfer scanner error over to the parser */
            s->e = t.e;

            /* Forward the error */
            parser_process_error (s);

            /* We know that the scanner MUST emit a valid op code token */
            /* So the next token must be valid */
            s->in_err = false;

#ifndef NDEBUG
            s->expect_opcode = true; /* Next token MUST be an op code with valid statement */
#endif

            continue;
          }
        default:
          {
            break;
          }
        }

      /* Consume the token */
      parser_err_t_wrap (parser_result_parse (&s->parser, t, &s->e), s);

      /* Check if parser is done */
      if (s->parser.done)
        {
          parser_err_t_wrap (parser_result_parse (&s->parser, quick_tok (0), &s->e), s);

          /* Consume query */
          struct statement *next = parser_result_consume (&s->parser);

          /* Reset allocator */
          lalloc_reset (&s->parser_work);

          /* Write to output buffer */
          parser_write_result (s, next);
        }
    }

  return SUCCESS;
}

static void
execute_err (parser *s)
{
  DBG_ASSERT (parser_steady_state, s);
  ASSERT (s->in_err);

  /* Read until you see a semi colon */
  struct token t;
  while (cbuffer_read (&t, sizeof t, 1, s->input))
    {
      i_log_info ("Parser (Error state) recieved token: %s\n", tt_tostr (t.type));
      if (t.type == TT_SEMICOLON)
        {
          s->in_err = false;
          return;
        }
    }
}

static err_t
execute_one_token (parser *p)
{
  DBG_ASSERT (parser_steady_state, p);

  if (p->in_err)
    {
      execute_err (p);
      return SUCCESS;
    }
  else
    {
      return execute_normal (p);
    }
}

void
parser_init (
    parser *dest,
    struct cbuffer *input,
    struct cbuffer *output)
{
  ASSERT (dest);
  ASSERT (input);
  ASSERT (cbuffer_avail (input) % sizeof (struct token) == 0);
  ASSERT (output);
  ASSERT (cbuffer_avail (output) % sizeof (struct statement_result) == 0);

  *dest = (parser){
    .input = input,
    .output = output,
    .parser = { 0 },
    .e = error_create (),
    .parser_work = lalloc_create_from (dest->_parser_work),
    .in_err = false,

#ifndef NDEBUG
    .expect_opcode = false,
#endif
  };
}

void
parser_execute (parser *p)
{
  DBG_ASSERT (parser, p);

  while (true)
    {
      /**
       * Block on downstream
       */
      if (cbuffer_avail (p->output) < sizeof (struct query))
        {
          return;
        }

      /**
       * Block on upstream
       */
      if (cbuffer_len (p->input) < sizeof (struct token))
        {
          return;
        }

      parser_err_t_continue (execute_one_token (p), p);
    }
}

/**
 #ifndef NTEST
   void
   test_parser_case (const token *input, const query *expected_output, u32 ilen, u32 olen)
   {
   parser p;

   token _tokens[20];
   query _queries[10];

   cbuffer tokens = cbuffer_create_from (_tokens);
   cbuffer queries = cbuffer_create_from (_queries);

   parser_init (&p, &tokens, &queries);

   // expected_output i
   u32 ii = 0;
   u32 oi = 0;

   while (true)
    {
      // Seed scanner
      if (ii < ilen)
        {
          ii += cbuffer_write (input + ii, sizeof *input, ilen - ii, &tokens);
        }
      else
        {
          break;
        }

      // Execute scanner
      parser_execute (&p);

      // Check resulting tokens
      query left;
      while (cbuffer_read (&left, sizeof left, 1, &queries))
        {
          test_fail_if (oi > olen);
          query right = expected_output[oi];

          if (!query_equal (&left, &right))
            {
              i_log_failure ("Actual and expected queries do not equal each other\n");
              i_log_failure ("Actual: \n");
              i_log_query (left);
              i_log_failure ("Expected: \n");
              i_log_query (right);
            }
          test_assert ( query_equal (&left, &right));
          oi++;
        }
    }
   }
 #endif

#ifndef NTEST
   TEST (TT_UNIT,parser)
   {
   error e = error_create ();

   query actual;
   query expected;

   test_fail_if_null (qspce);
   test_fail_if (query_provider_get (qspce, &actual, QT_CREATE, &e));
   test_fail_if (query_provider_get (qspce, &expected, QT_CREATE, &e));

   expected.create->vname = strfcstr ("foo");
   expected.create->type = (type){
    .type = T_PRIM,
    .p = U32,
   };

   const token tk1[] = {
    tt_opcode (TT_CREATE, actual),
    tt_ident (strfcstr ("foo")),
    tt_prim (U32),
    quick_tok (TT_SEMICOLON),
   };

   test_parser_case (tk1, (query[]){ expected }, 4, 1);
   }
#endif
 */
