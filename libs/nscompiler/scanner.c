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
 *   Implements scanner.h. Provides lexical analysis implementation with
 *   state machine for tokenizing input, recognizing keywords, identifiers,
 *   operators, strings, numbers, and handling error recovery.
 */

#include <numstore/compiler/scanner.h>

#include <numstore/compiler/statement.h>
#include <numstore/compiler/tokens.h>
#include <numstore/core/assert.h>
#include <numstore/core/cbuffer.h>
#include <numstore/core/numbers.h>
#include <numstore/core/strings_utils.h>
#include <numstore/intf/logging.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    scanner, scanner, s,
    {
      ASSERT (s);
    })

DEFINE_DBG_ASSERT (
    scanner, scanner_steady_state, s,
    {
      DBG_ASSERT (scanner, s);
    })

DEFINE_DBG_ASSERT (
    scanner, scanner_right_after_error, s,
    {
      DBG_ASSERT (scanner, s);
    })

////////////////////////////////////////////////////////////
// PREMADE MAGIC STRINGS

typedef struct
{
  const struct string token;
  struct token t;
} magic_token;

#define STR_LIT(s)        \
  {                       \
    (sizeof (s) - 1), (s) \
  }

// TODO Implement a trie for faster searching

static const magic_token op_codes[] = {
  /* JSON commands */
  { .token = STR_LIT ("create"), .t = { .type = TT_CREATE } },
  { .token = STR_LIT ("delete"), .t = { .type = TT_DELETE } },
  { .token = STR_LIT ("insert"), .t = { .type = TT_INSERT } },
};

static const magic_token magic_tokens[] = {
  /* types */
  { .token = STR_LIT ("struct"), .t = { .type = TT_STRUCT } },
  { .token = STR_LIT ("union"), .t = { .type = TT_UNION } },
  { .token = STR_LIT ("enum"), .t = { .type = TT_ENUM } },
  { .token = STR_LIT ("u8"), .t = { .type = TT_PRIM, .prim = U8 } },
  { .token = STR_LIT ("u16"), .t = { .type = TT_PRIM, .prim = U16 } },
  { .token = STR_LIT ("u32"), .t = { .type = TT_PRIM, .prim = U32 } },
  { .token = STR_LIT ("u64"), .t = { .type = TT_PRIM, .prim = U64 } },
  { .token = STR_LIT ("i8"), .t = { .type = TT_PRIM, .prim = I8 } },
  { .token = STR_LIT ("i16"), .t = { .type = TT_PRIM, .prim = I16 } },
  { .token = STR_LIT ("i32"), .t = { .type = TT_PRIM, .prim = I32 } },
  { .token = STR_LIT ("i64"), .t = { .type = TT_PRIM, .prim = I64 } },
  { .token = STR_LIT ("f16"), .t = { .type = TT_PRIM, .prim = F16 } },
  { .token = STR_LIT ("f32"), .t = { .type = TT_PRIM, .prim = F32 } },
  { .token = STR_LIT ("f64"), .t = { .type = TT_PRIM, .prim = F64 } },
  { .token = STR_LIT ("f128"), .t = { .type = TT_PRIM, .prim = F128 } },
  { .token = STR_LIT ("cf32"), .t = { .type = TT_PRIM, .prim = CF32 } },
  { .token = STR_LIT ("cf64"), .t = { .type = TT_PRIM, .prim = CF64 } },
  { .token = STR_LIT ("cf128"), .t = { .type = TT_PRIM, .prim = CF128 } },
  { .token = STR_LIT ("cf256"), .t = { .type = TT_PRIM, .prim = CF256 } },
  { .token = STR_LIT ("ci16"), .t = { .type = TT_PRIM, .prim = CI16 } },
  { .token = STR_LIT ("ci32"), .t = { .type = TT_PRIM, .prim = CI32 } },
  { .token = STR_LIT ("ci64"), .t = { .type = TT_PRIM, .prim = CI64 } },
  { .token = STR_LIT ("ci128"), .t = { .type = TT_PRIM, .prim = CI128 } },
  { .token = STR_LIT ("cu16"), .t = { .type = TT_PRIM, .prim = CU16 } },
  { .token = STR_LIT ("cu32"), .t = { .type = TT_PRIM, .prim = CU32 } },
  { .token = STR_LIT ("cu64"), .t = { .type = TT_PRIM, .prim = CU64 } },
  { .token = STR_LIT ("cu128"), .t = { .type = TT_PRIM, .prim = CU128 } },

  { .token = STR_LIT ("true"), .t = { .type = TT_TRUE } },
  { .token = STR_LIT ("false"), .t = { .type = TT_FALSE } },
};

////////////////////////////////////////////////////////////
// STATE MACHINE FUNCTIONS

static err_t execute_at_most_one_token (scanner *s);
static err_t execute_at_most_one_token_start (scanner *s);
static err_t execute_at_most_one_token_ident (scanner *s);
static err_t execute_at_most_one_token_string (scanner *s);
static err_t execute_at_most_one_token_number (scanner *s);
static err_t execute_at_most_one_token_dec (scanner *s);
static void execute_at_most_one_token_err (scanner *s);

////////////////////////////////////////////////////////////
// UTILS

#define scanner_err_t_wrap(expr, c)  \
  do                                 \
    {                                \
      err_t __ret = (err_t)expr;     \
      if (__ret < SUCCESS)           \
        {                            \
          scanner_process_error (c); \
          return __ret;              \
        }                            \
    }                                \
  while (0)

#define scanner_err_t_continue(expr, c) \
  do                                    \
    {                                   \
      err_t __ret = (err_t)expr;        \
      if (__ret < SUCCESS)              \
        {                               \
          scanner_process_error (c);    \
        }                               \
    }                                   \
  while (0)

static inline err_t
scanner_process_token (scanner *s, struct token t)
{
  DBG_ASSERT (scanner_steady_state, s);

  i_log_trace ("Scanner processing token: %s\n", tt_tostr (t.type));
  cbuffer_write_expect (&t, sizeof t, 1, s->output);

  s->state.state = SS_START;

  return SUCCESS;
}

static inline void
scanner_process_error (scanner *s)
{
  /**
   * Only need to process error once,
   * otherwise, continue in rewinding state
   */
  if (s->state.state != SS_ERR)
    {
      DBG_ASSERT (scanner_right_after_error, s);
      /* Set state to rewind */
      s->state.state = SS_ERR;

      /* Write error out */
      struct token t = tt_err (s->e);
      i_log_trace ("Scanner processing token: %s\n", tt_tostr (t.type));
      cbuffer_write_expect (&t, sizeof t, 1, s->output);

      /* Reset error and statement */
      s->e = error_create ();
      s->current = NULL;
    }

  DBG_ASSERT (scanner_steady_state, s);
}

static inline err_t
scanner_push_char (scanner *s, char next)
{
  DBG_ASSERT (scanner_steady_state, s);

  if (s->state.slen == 512)
    {
      scanner_err_t_wrap (error_causef (&s->e, ERR_NOMEM, "internal buffer overflow"), s);
      UNREACHABLE ();
    }

  s->state.str[s->state.slen++] = next;

  return SUCCESS;
}

static inline void
scanner_advance_expect (scanner *s)
{
  DBG_ASSERT (scanner_steady_state, s);
  u8 next;
  cbuffer_pop_front_expect (&next, 1, s->input);
}

static inline err_t
scanner_cpy_advance_expect (scanner *s)
{
  DBG_ASSERT (scanner_steady_state, s);

  /* Dequeue and copy */
  u8 next;
  cbuffer_pop_front_expect (&next, 1, s->input);

  scanner_err_t_wrap (scanner_push_char (s, next), s);

  return SUCCESS;
}

static inline bool
scanner_peek (u8 *dest, scanner *s)
{
  DBG_ASSERT (scanner_steady_state, s);
  return cbuffer_peek_front (dest, 1, s->input);
}

static inline void
consume_whitespace (scanner *s)
{
  DBG_ASSERT (scanner_steady_state, s);
  u8 c;

  while (scanner_peek (&c, s))
    {
      switch (c)
        {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
          scanner_advance_expect (s);
          break;
        default:
          return;
        }
    }
}

//////////////////////////// State Machine Functions
static err_t
execute_at_most_one_token (scanner *s)
{
  DBG_ASSERT (scanner_steady_state, s);

  /* Pick up where you left off */
  switch (s->state.state)
    {
    case SS_START:
      {
        return execute_at_most_one_token_start (s);
      }
    case SS_IDENT:
      {
        return execute_at_most_one_token_ident (s);
      }
    case SS_NUMBER:
      {
        return execute_at_most_one_token_number (s);
      }
    case SS_DECIMAL:
      {
        return execute_at_most_one_token_dec (s);
      }
    case SS_STRING:
      {
        return execute_at_most_one_token_string (s);
      }
    case SS_ERR:
      {
        execute_at_most_one_token_err (s);
        return SUCCESS;
      }
    default:
      {
        UNREACHABLE ();
      }
    }
}

static inline err_t
scanner_commit_string (struct string *dest, scanner *s)
{
  DBG_ASSERT (scanner_steady_state, s);

  /* Can only transfer strings if we're inside of a statement */
  if (s->current == NULL)
    {
      scanner_err_t_wrap (
          error_causef (
              &s->e, ERR_SYNTAX,
              "No statement to allocate string: %.*s",
              s->state.slen, s->state.str),
          s);
      UNREACHABLE ();
    }

  /* Allocate onto the query space */
  char *ret = lmalloc (&s->current->qspace, 1, s->state.slen, &s->e);
  if (ret == NULL)
    {
      scanner_err_t_wrap (s->e.cause_code, s);
      UNREACHABLE ();
    }

  /* Copy over local string to query space */
  i_memcpy (ret, s->state.str, s->state.slen);
  *dest = (struct string){
    .data = ret,
    .len = s->state.slen,
  };

  /* Reset local string */
  s->state.slen = 0;

  return SUCCESS;
}

static err_t
process_string_or_ident (scanner *s, enum token_t ttype)
{
  DBG_ASSERT (scanner_steady_state, s);
  ASSERT (ttype == TT_STRING || ttype == TT_IDENTIFIER);

  struct string lit;

  scanner_err_t_wrap (scanner_commit_string (&lit, s), s);

  /* Process the token */
  struct token t = (struct token){
    .type = ttype,
    .str = lit,
  };

  return scanner_process_token (s, t);
}

static err_t
process_maybe_ident (scanner *s)
{
  DBG_ASSERT (scanner_steady_state, s);

  ASSERT (s->state.slen > 0);

  struct string lit = (struct string){
    .data = s->state.str,
    .len = s->state.slen,
  };

  /* Check for magic tokens */
  for (u32 i = 0; i < arrlen (magic_tokens); ++i)
    {
      if (string_equal (lit, magic_tokens[i].token))
        {
          /* reset (lit is useless) */
          s->state.slen = 0;
          return scanner_process_token (s, magic_tokens[i].t);
        }
    }

  /* Check for op codes */
  for (u32 i = 0; i < arrlen (op_codes); ++i)
    {
      if (string_equal (lit, op_codes[i].token))
        {
          struct token t = op_codes[i].t;

          /* reset (lit is useless) */
          s->state.slen = 0;

          /* Create a new statement */
          if (s->current == NULL)
            {
              s->current = statement_create (&s->e);
              if (s->current == NULL)
                {
                  scanner_err_t_wrap (s->e.cause_code, s);
                  UNREACHABLE ();
                }
            }

          return scanner_process_token (s, t);
        }
    }

  /* Otherwise it's a TT_IDENTIFIER */
  return process_string_or_ident (s, TT_IDENTIFIER);
}

static err_t
execute_at_most_one_token_ident (scanner *s)
{
  DBG_ASSERT (scanner_steady_state, s);
  ASSERT (s->state.state == SS_IDENT);

  u8 c;

  while (scanner_peek (&c, s))
    {
      if (!is_alpha_num (c))
        {
          return process_maybe_ident (s);
        }

      scanner_err_t_wrap (scanner_cpy_advance_expect (s), s);
    }

  return SUCCESS; /* no more chars to consume */
}

TODO ("Support backslash in strings")
static err_t
execute_at_most_one_token_string (scanner *s)
{
  DBG_ASSERT (scanner_steady_state, s);
  ASSERT (s->state.state == SS_STRING);

  u8 c;

  while (scanner_peek (&c, s))
    {
      if (c == '"')
        {
          scanner_advance_expect (s); /* Skip over (last) quote */
          return process_string_or_ident (s, TT_STRING);
        }

      scanner_err_t_wrap (scanner_cpy_advance_expect (s), s);
    }

  return SUCCESS;
}

static inline err_t
process_dec (scanner *s)
{
  DBG_ASSERT (scanner_steady_state, s);

  const struct string lit = (struct string){
    .data = s->state.str,
    .len = s->state.slen,
  };
  s->state.slen = 0;

  /* Parse the float */
  f32 dest;
  scanner_err_t_wrap (parse_f32_expect (&dest, lit, &s->e), s);

  scanner_err_t_wrap (scanner_process_token (s, tt_float (dest)), s);

  return SUCCESS;
}

static err_t
execute_at_most_one_token_dec (scanner *s)
{
  DBG_ASSERT (scanner_steady_state, s);
  ASSERT (s->state.state == SS_DECIMAL);

  u8 c;
  while (scanner_peek (&c, s))
    {
      if (!is_num (c))
        {
          return process_dec (s);
        }
      scanner_err_t_wrap (scanner_cpy_advance_expect (s), s);
    }

  return SUCCESS;
}

static void
execute_at_most_one_token_err (scanner *s)
{
  DBG_ASSERT (scanner_steady_state, s);
  ASSERT (s->state.state == SS_ERR);

  u8 c;
  while (scanner_peek (&c, s))
    {
      if (c == ';')
        {
          scanner_advance_expect (s); /* Skip over the ; */
          s->state.state = SS_START;
          return;
        }
    }
}

static inline err_t
process_int (scanner *s)
{
  DBG_ASSERT (scanner_steady_state, s);

  const struct string lit = (struct string){
    .data = s->state.str,
    .len = s->state.slen,
  };
  s->state.slen = 0;

  /* Parse the int */
  i32 dest;
  scanner_err_t_wrap (parse_i32_expect (&dest, lit, &s->e), s);

  scanner_err_t_wrap (scanner_process_token (s, tt_integer (dest)), s);

  return SUCCESS;
}

static err_t
execute_at_most_one_token_number (scanner *s)
{
  DBG_ASSERT (scanner_steady_state, s);
  ASSERT (s->state.state == SS_NUMBER);

  u8 c;

  while (scanner_peek (&c, s))
    {
      if (!is_num (c))
        {
          if (c == '.')
            {
              s->state.state = SS_DECIMAL;
              scanner_err_t_wrap (scanner_cpy_advance_expect (s), s);
              return execute_at_most_one_token_dec (s);
            }
          else
            {
              return process_int (s);
            }
        }
      scanner_err_t_wrap (scanner_cpy_advance_expect (s), s);
    }

  return SUCCESS;
}

static err_t
execute_at_most_one_token_start (scanner *s)
{
  DBG_ASSERT (scanner_steady_state, s);
  ASSERT (s->state.state == SS_START);

  u8 next;
  bool more = scanner_peek (&next, s);
  ASSERT (more); /* Shouldn't enter this function with an empty input buffer */

  /**
   * First, check if previous token is set
   */
  char prev = s->prev_token;
  s->prev_token = -1;
  switch (prev)
    {
    case -1:
      {
        /* No previous token, continue to process current token */
        break;
      }
    case '!':
      {
        switch (next)
          {
          case '=':
            {
              /* Found !=, consume both tokens */
              scanner_advance_expect (s);
              scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_BANG_EQUAL)), s);
              return SUCCESS;
            }
          default:
            {
              /* Just !, consume previous token */
              scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_BANG)), s);

              /* Continue to process current token */
              break;
            }
          }
        break;
      }
    case '=':
      {
        switch (next)
          {
          case '=':
            {
              /* Found ==, consume both tokens */
              scanner_advance_expect (s);
              scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_EQUAL_EQUAL)), s);
              return SUCCESS;
            }
          default:
            {
              /* Bare = is invalid */
              scanner_err_t_wrap (
                  error_causef (
                      &s->e,
                      ERR_SYNTAX,
                      "Expected ==, got ="),
                  s);
              UNREACHABLE ();
            }
          }
        break;
      }

    case '>':
      {
        switch (next)
          {
          case '=':
            {
              /* Found >=, consume both tokens */
              scanner_advance_expect (s);
              scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_GREATER_EQUAL)), s);
              return SUCCESS;
            }
          default:
            {
              /* Just >, consume previous token */
              scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_GREATER)), s);

              /* Continue to process current token */
              break;
            }
          }
        break;
      }

    case '<':
      {
        switch (next)
          {
          case '=':
            {
              /* Found <=, consume both tokens */
              scanner_advance_expect (s);
              scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_LESS_EQUAL)), s);
              return SUCCESS;
            }
          default:
            {
              /* Just <, consume previous token */
              scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_LESS)), s);

              /* Continue to process current token */
              break;
            }
          }
        break;
      }
    case '&':
      {
        switch (next)
          {
          case '&':
            {
              /* Found !|, consume both tokens */
              scanner_advance_expect (s);
              scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_AMPERSAND_AMPERSAND)), s);
              return SUCCESS;
            }
          default:
            {
              /* Just &, consume previous token */
              scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_AMPERSAND)), s);

              /* Continue to process current token */
              break;
            }
          }
        break;
      }
    case '|':
      {
        switch (next)
          {
          case '|':
            {
              /* Found !|, consume both tokens */
              scanner_advance_expect (s);
              scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_PIPE_PIPE)), s);
              return SUCCESS;
            }
          default:
            {
              /* Just |, consume previous token */
              scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_PIPE)), s);

              /* Continue to process current token */
              break;
            }
          }
        break;
      }
    }

  /* Next token - consume any white space */
  consume_whitespace (s);

  if (!scanner_peek (&next, s))
    {
      /* no more tokens (could happen if we have a ton of whitespace) */
      return SUCCESS;
    }

  /* Regular processing */
  switch (next)
    {
    /**
     * In all the 1 tokens, process then stay in SS_START for next
     */

    /*      Arithmetic Operators */
    case '+':
      {
        scanner_advance_expect (s);
        scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_PLUS)), s);
        break;
      }
    case '-':
      {
        scanner_advance_expect (s);
        scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_MINUS)), s);
        break;
      }
    case '/':
      {
        scanner_advance_expect (s);
        scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_SLASH)), s);
        break;
      }
    case '*':
      {
        scanner_advance_expect (s);
        scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_STAR)), s);
        break;
      }

    /*      Logical Operators */
    case '!':
      {
        scanner_advance_expect (s);
        s->prev_token = '!';
        break; /* Handled next execute */
      }
    case '=':
      {
        scanner_advance_expect (s);
        s->prev_token = '=';
        break; /* Handled next execute */
      }
    case '>':
      {
        scanner_advance_expect (s);
        s->prev_token = '>';
        break; /* Handled next execute */
      }
    case '<':
      {
        scanner_advance_expect (s);
        s->prev_token = '<';
        break; /* Handled next execute */
      }

    /*       Fancy Operators */
    case '~':
      {
        scanner_advance_expect (s);
        scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_NOT)), s);
        break;
      }
    case '^':
      {
        scanner_advance_expect (s);
        scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_CARET)), s);
        break;
      }
    case '%':
      {
        scanner_advance_expect (s);
        scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_PERCENT)), s);
        break;
      }
    case '|':
      {
        scanner_advance_expect (s);
        s->prev_token = '|';
        break; /* Handled next execute */
      }
    case '&':
      {
        scanner_advance_expect (s);
        s->prev_token = '&';
        break; /* Handled next execute */
      }

    /*        Other One char tokens */
    case ';':
      {
        scanner_advance_expect (s);
        scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_SEMICOLON)), s);

        /* Let go of the current statement */
        s->current = NULL;
        break;
      }
    case ':':
      {
        scanner_advance_expect (s);
        scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_COLON)), s);
        break;
      }
    case '[':
      {
        scanner_advance_expect (s);
        scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_LEFT_BRACKET)), s);
        break;
      }
    case ']':
      {
        scanner_advance_expect (s);
        scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_RIGHT_BRACKET)), s);
        break;
      }
    case '{':
      {
        scanner_advance_expect (s);
        scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_LEFT_BRACE)), s);
        break;
      }
    case '}':
      {
        scanner_advance_expect (s);
        scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_RIGHT_BRACE)), s);
        break;
      }
    case '(':
      {
        scanner_advance_expect (s);
        scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_LEFT_PAREN)), s);
        break;
      }
    case ')':
      {
        scanner_advance_expect (s);
        scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_RIGHT_PAREN)), s);
        break;
      }
    case ',':
      {
        scanner_advance_expect (s);
        scanner_err_t_wrap (scanner_process_token (s, quick_tok (TT_COMMA)), s);
        break;
      }

    /* TT_STRING */
    case '"':
      {
        s->state.slen = 0;
        scanner_advance_expect (s); /* Skip the '"' */
        s->state.state = SS_STRING;
        break;
      }

    default:
      {
        /* TT_IDENTIFIER */
        if (is_alpha (next))
          {
            /*
             * Due to maximal munch all magic strings
             * are treated as ident - then checked at the end
             *
             * Advance forward once - because strings can start with
             * alpha, then steady state processes alpha numeric
             */
            s->state.slen = 0;
            scanner_err_t_wrap (scanner_cpy_advance_expect (s), s);
            s->state.state = SS_IDENT;
            break;
          }

        /* TT_INTEGER */
        /* TT_FLOAT */
        else if (is_num (next))
          {
            /**
             * Advance forward once because steady state doesn't
             * account for + or -
             */
            s->state.slen = 0;
            scanner_err_t_wrap (scanner_cpy_advance_expect (s), s);
            s->state.state = SS_NUMBER;
            break;
          }
        else
          {
            scanner_advance_expect (s);
            scanner_err_t_wrap (
                error_causef (
                    &s->e, ERR_SYNTAX,
                    "Unexpected char: %c",
                    next),
                s);
            UNREACHABLE ();
          }
      }
    }
  return SUCCESS;
}

////////////////////////////////////////////////////////////
// MAIN FUNCTIONS

void
scanner_init (scanner *dest, struct cbuffer *input, struct cbuffer *output)
{
  ASSERT (dest);
  ASSERT (input);
  ASSERT (output);
  ASSERT (cbuffer_avail (output) % sizeof (struct token) == 0);

  *dest = (scanner){
    .state = {
        .state = SS_START,
        .slen = 0,
    },
    .input = input,
    .output = output,

    .prev_token = -1,
    .e = error_create (),

    .current = NULL,
  };

  DBG_ASSERT (scanner_steady_state, dest);
}

void
scanner_execute (scanner *s)
{
  DBG_ASSERT (scanner_steady_state, s);

  while (true)
    {
      /**
       * Block on downstream
       */
      if (cbuffer_avail (s->output) < sizeof (struct token))
        {
          return;
        }

      /**
       * Block on upstream
       */
      if (cbuffer_len (s->input) < sizeof (char))
        {
          return;
        }

      scanner_err_t_continue (execute_at_most_one_token (s), s);
    }
}

#ifndef NTEST
static void
test_scanner_case (const char *input, const struct token *expected_output, u32 ilen, u32 olen)
{
  scanner s;

  char _chars[20];
  struct token _tokens[20];

  struct cbuffer chars = cbuffer_create_from (_chars);
  struct cbuffer tokens = cbuffer_create_from (_tokens);

  scanner_init (&s, &chars, &tokens);

  error e = error_create ();
  struct statement *stmt = statement_create (&e);
  s.current = stmt;
  test_fail_if_null (s.current);

  /* expected_output i */
  u32 ii = 0;
  u32 oi = 0;

  while (true)
    {
      /* Seed scanner */
      if (ii < ilen)
        {
          ii += cbuffer_write (input + ii, 1, ilen - ii, &chars);
        }
      else
        {
          break;
        }

      /* Execute scanner */
      scanner_execute (&s);

      /* Check resulting tokens */
      struct token left;
      while (cbuffer_read (&left, sizeof left, 1, &tokens))
        {
          test_fail_if (oi > olen);
          struct token right = expected_output[oi];

          /* Don't deep compare op codes - they get strung */
          /* together on the parser */
          if (tt_is_opcode (left.type))
            {
              test_assert_int_equal (left.type, right.type);
            }
          else
            {
              if (!token_equal (&left, &right))
                {
                  i_log_failure ("Input str: %s\n", input);
                  i_log_failure ("Failed at input token: %s expected %s\n",
                                 tt_tostr (left.type), tt_tostr (right.type));
                }
              test_assert (token_equal (&left, &right));
            }
          oi++;
        }
    }
  statement_free (stmt);
}

TEST (TT_UNIT, scanner_two_char_tokens)
{
  char *src = "! ! ! ! != != != ! != != != != == ! < ! <= ! > ! >= == == == ! == < == <= == "
              "!= == == < < < <= <= < <= <= < < <= < < < > > > >= > > == >= >= > >= > >= >= > "
              ">= >= > >= > > >= >= > >= > ! == == == != < > ! ! >= != ! == < < != == <= >= == "
              "> > ! != < > == != != == < <= >= == != == < != == >= > > < != < > == > > < < > > "
              ", ! , != , == , < , <= , >= , >";

  struct token expected_tokens[] = {
    quick_tok (TT_BANG),
    quick_tok (TT_BANG),
    quick_tok (TT_BANG),
    quick_tok (TT_BANG),
    quick_tok (TT_BANG_EQUAL),
    quick_tok (TT_BANG_EQUAL),
    quick_tok (TT_BANG_EQUAL),
    quick_tok (TT_BANG),
    quick_tok (TT_BANG_EQUAL),
    quick_tok (TT_BANG_EQUAL),
    quick_tok (TT_BANG_EQUAL),
    quick_tok (TT_BANG_EQUAL),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_BANG),
    quick_tok (TT_LESS),
    quick_tok (TT_BANG),
    quick_tok (TT_LESS_EQUAL),
    quick_tok (TT_BANG),
    quick_tok (TT_GREATER),
    quick_tok (TT_BANG),
    quick_tok (TT_GREATER_EQUAL),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_BANG),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_LESS),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_LESS_EQUAL),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_BANG_EQUAL),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_LESS),
    quick_tok (TT_LESS),
    quick_tok (TT_LESS),
    quick_tok (TT_LESS_EQUAL),
    quick_tok (TT_LESS_EQUAL),
    quick_tok (TT_LESS),
    quick_tok (TT_LESS_EQUAL),
    quick_tok (TT_LESS_EQUAL),
    quick_tok (TT_LESS),
    quick_tok (TT_LESS),
    quick_tok (TT_LESS_EQUAL),
    quick_tok (TT_LESS),
    quick_tok (TT_LESS),
    quick_tok (TT_LESS),
    quick_tok (TT_GREATER),
    quick_tok (TT_GREATER),
    quick_tok (TT_GREATER),
    quick_tok (TT_GREATER_EQUAL),
    quick_tok (TT_GREATER),
    quick_tok (TT_GREATER),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_GREATER_EQUAL),
    quick_tok (TT_GREATER_EQUAL),
    quick_tok (TT_GREATER),
    quick_tok (TT_GREATER_EQUAL),
    quick_tok (TT_GREATER),
    quick_tok (TT_GREATER_EQUAL),
    quick_tok (TT_GREATER_EQUAL),
    quick_tok (TT_GREATER),
    quick_tok (TT_GREATER_EQUAL),
    quick_tok (TT_GREATER_EQUAL),
    quick_tok (TT_GREATER),
    quick_tok (TT_GREATER_EQUAL),
    quick_tok (TT_GREATER),
    quick_tok (TT_GREATER),
    quick_tok (TT_GREATER_EQUAL),
    quick_tok (TT_GREATER_EQUAL),
    quick_tok (TT_GREATER),
    quick_tok (TT_GREATER_EQUAL),
    quick_tok (TT_GREATER),
    quick_tok (TT_BANG),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_BANG_EQUAL),
    quick_tok (TT_LESS),
    quick_tok (TT_GREATER),
    quick_tok (TT_BANG),
    quick_tok (TT_BANG),
    quick_tok (TT_GREATER_EQUAL),
    quick_tok (TT_BANG_EQUAL),
    quick_tok (TT_BANG),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_LESS),
    quick_tok (TT_LESS),
    quick_tok (TT_BANG_EQUAL),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_LESS_EQUAL),
    quick_tok (TT_GREATER_EQUAL),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_GREATER),
    quick_tok (TT_GREATER),
    quick_tok (TT_BANG),
    quick_tok (TT_BANG_EQUAL),
    quick_tok (TT_LESS),
    quick_tok (TT_GREATER),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_BANG_EQUAL),
    quick_tok (TT_BANG_EQUAL),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_LESS),
    quick_tok (TT_LESS_EQUAL),
    quick_tok (TT_GREATER_EQUAL),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_BANG_EQUAL),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_LESS),
    quick_tok (TT_BANG_EQUAL),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_GREATER_EQUAL),
    quick_tok (TT_GREATER),
    quick_tok (TT_GREATER),
    quick_tok (TT_LESS),
    quick_tok (TT_BANG_EQUAL),
    quick_tok (TT_LESS),
    quick_tok (TT_GREATER),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_GREATER),
    quick_tok (TT_GREATER),
    quick_tok (TT_LESS),
    quick_tok (TT_LESS),
    quick_tok (TT_GREATER),
    quick_tok (TT_GREATER),
    quick_tok (TT_COMMA),
    quick_tok (TT_BANG),
    quick_tok (TT_COMMA),
    quick_tok (TT_BANG_EQUAL),
    quick_tok (TT_COMMA),
    quick_tok (TT_EQUAL_EQUAL),
    quick_tok (TT_COMMA),
    quick_tok (TT_LESS),
    quick_tok (TT_COMMA),
    quick_tok (TT_LESS_EQUAL),
    quick_tok (TT_COMMA),
    quick_tok (TT_GREATER_EQUAL),
    quick_tok (TT_COMMA),
    quick_tok (TT_GREATER),
  };

  test_scanner_case (src, expected_tokens, i_strlen (src), arrlen (expected_tokens));
}

/* Various edge cases */
#define single_tok_edge_test_case(lit, tok_expr)                                                                          \
  do                                                                                                                      \
    {                                                                                                                     \
      src = lit;                                                                                                          \
      test_scanner_case (src, (struct token[]){ tok_expr }, i_strlen (src), 1);                                           \
      src = " " lit;                                                                                                      \
      test_scanner_case (src, (struct token[]){ tok_expr }, i_strlen (src), 1);                                           \
      src = lit " ";                                                                                                      \
      test_scanner_case (src, (struct token[]){ tok_expr }, i_strlen (src), 1);                                           \
      src = " " lit " ";                                                                                                  \
      test_scanner_case (src, (struct token[]){ tok_expr }, i_strlen (src), 1);                                           \
      src = " " lit " ";                                                                                                  \
      test_scanner_case (src, (struct token[]){ tok_expr }, i_strlen (src), 1);                                           \
      src = "+" lit;                                                                                                      \
      test_scanner_case (src, (struct token[]){ quick_tok (TT_PLUS), tok_expr }, i_strlen (src), 2);                      \
      src = "+ " lit;                                                                                                     \
      test_scanner_case (src, (struct token[]){ quick_tok (TT_PLUS), tok_expr }, i_strlen (src), 2);                      \
      src = lit "+";                                                                                                      \
      test_scanner_case (src, (struct token[]){ tok_expr, quick_tok (TT_PLUS) }, i_strlen (src), 2);                      \
      src = lit " +";                                                                                                     \
      test_scanner_case (src, (struct token[]){ tok_expr, quick_tok (TT_PLUS) }, i_strlen (src), 2);                      \
      src = "+" lit "+";                                                                                                  \
      test_scanner_case (src, (struct token[]){ quick_tok (TT_PLUS), tok_expr, quick_tok (TT_PLUS) }, i_strlen (src), 2); \
      src = "+ " lit " +";                                                                                                \
      test_scanner_case (src, (struct token[]){ quick_tok (TT_PLUS), tok_expr, quick_tok (TT_PLUS) }, i_strlen (src), 2); \
    }                                                                                                                     \
  while (0)

TEST (TT_UNIT, scanner_single_tokens)
{
  char *src;

  single_tok_edge_test_case ("+", quick_tok (TT_PLUS));
  single_tok_edge_test_case ("-", quick_tok (TT_MINUS));
  single_tok_edge_test_case ("/", quick_tok (TT_SLASH));
  single_tok_edge_test_case ("*", quick_tok (TT_STAR));

  /*      Logical Operators */
  single_tok_edge_test_case ("!", quick_tok (TT_BANG));
  single_tok_edge_test_case ("!=", quick_tok (TT_BANG_EQUAL));
  single_tok_edge_test_case ("==", quick_tok (TT_EQUAL_EQUAL));
  single_tok_edge_test_case (">", quick_tok (TT_GREATER));
  single_tok_edge_test_case (">=", quick_tok (TT_GREATER_EQUAL));
  single_tok_edge_test_case ("<", quick_tok (TT_LESS));
  single_tok_edge_test_case ("<=", quick_tok (TT_LESS_EQUAL));

  /*       Fancy Operators */
  single_tok_edge_test_case ("~", quick_tok (TT_NOT));
  single_tok_edge_test_case ("^", quick_tok (TT_CARET));
  single_tok_edge_test_case ("%", quick_tok (TT_PERCENT));
  single_tok_edge_test_case ("|", quick_tok (TT_PIPE));
  single_tok_edge_test_case ("||", quick_tok (TT_PIPE_PIPE));
  single_tok_edge_test_case ("&", quick_tok (TT_AMPERSAND));
  single_tok_edge_test_case ("&&", quick_tok (TT_AMPERSAND_AMPERSAND));

  /*        Other One char tokens */
  single_tok_edge_test_case (";", quick_tok (TT_SEMICOLON));
  single_tok_edge_test_case (":", quick_tok (TT_COLON));
  single_tok_edge_test_case ("[", quick_tok (TT_LEFT_BRACKET));
  single_tok_edge_test_case ("]", quick_tok (TT_RIGHT_BRACKET));
  single_tok_edge_test_case ("{", quick_tok (TT_LEFT_BRACE));
  single_tok_edge_test_case ("}", quick_tok (TT_RIGHT_BRACE));
  single_tok_edge_test_case ("(", quick_tok (TT_LEFT_PAREN));
  single_tok_edge_test_case (")", quick_tok (TT_RIGHT_PAREN));
  single_tok_edge_test_case (",", quick_tok (TT_COMMA));

  /*      Other */
  single_tok_edge_test_case ("\"foo\"", tt_string (strfcstr ("foo")));
  single_tok_edge_test_case ("bar", tt_ident (strfcstr ("bar")));

  /* Tokens that start with a number or +/- */
  single_tok_edge_test_case ("1234", tt_integer (1234));
  single_tok_edge_test_case ("123.456", tt_float (123.456));

  /*      Identifiers first, then lits identified later */
  /*      Literal Operations */
  single_tok_edge_test_case ("create", quick_tok (TT_CREATE));
  single_tok_edge_test_case ("delete", quick_tok (TT_DELETE));
  single_tok_edge_test_case ("insert", quick_tok (TT_INSERT));

  /*      Type lits */
  single_tok_edge_test_case ("struct", quick_tok (TT_STRUCT));
  single_tok_edge_test_case ("union", quick_tok (TT_UNION));
  single_tok_edge_test_case ("enum", quick_tok (TT_ENUM));
  PRIM_FOR_EACH_LITERAL (single_tok_edge_test_case, quick_tok (TT_PRIM));

  /*      Bools */
  single_tok_edge_test_case ("true", quick_tok (TT_TRUE));
  single_tok_edge_test_case ("false", quick_tok (TT_FALSE));

  /*      Special Error token on failed scan */
  single_tok_edge_test_case ("\"unterminated", quick_tok (TT_ERROR));
}

TEST_disabled (TT_UNIT, scanner_special)
{
  const char *src2 = "+";
  test_scanner_case (src2, (struct token[]){ quick_tok (TT_PLUS) }, strlen (src2), 1);

  const char *src3 = "!= == >= <=";
  test_scanner_case (src3, (struct token[]){
                               quick_tok (TT_BANG_EQUAL),
                               quick_tok (TT_EQUAL_EQUAL),
                               quick_tok (TT_GREATER_EQUAL),
                               quick_tok (TT_LESS_EQUAL),
                           },
                     strlen (src3), 4);

  const char *src4 = "a+b";
  test_scanner_case (src4, (struct token[]){
                               tt_ident (strfcstr ("a")),
                               quick_tok (TT_PLUS),
                               tt_ident (strfcstr ("b")),
                           },
                     strlen (src4), 3);

  const char *src5 = "0 1 23 +12 -34 56.78 0.9 1.0";
  test_scanner_case (src5, (struct token[]){
                               tt_integer (0),
                               tt_integer (1),
                               tt_integer (23),
                               quick_tok (TT_PLUS),
                               tt_integer (12),
                               quick_tok (TT_MINUS),
                               tt_integer (34),
                               tt_float (56.78f),
                               tt_float (0.9f),
                               tt_float (1.0f),
                           },
                     strlen (src5), 8);

  const char *src6 = "\"hello\" ";
  test_scanner_case (src6, (struct token[]){
                               tt_string (strfcstr ("hello")),
                               tt_string (strfcstr ("")),
                           },
                     strlen (src6), 2);

  const char *src7 = "create crate createx";
  test_scanner_case (src7, (struct token[]){
                               quick_tok (TT_CREATE),
                               tt_ident (strfcstr ("crate")),
                               tt_ident (strfcstr ("createx")),
                           },
                     strlen (src7), 3);

  const char *src8 = "true false";
  test_scanner_case (src8, (struct token[]){
                               quick_tok (TT_TRUE),
                               quick_tok (TT_FALSE),
                           },
                     strlen (src8), 2);

  const char *src9 = "struct union enum prim";
  test_scanner_case (src9, (struct token[]){
                               quick_tok (TT_STRUCT),
                               quick_tok (TT_UNION),
                               quick_tok (TT_ENUM),
                               quick_tok (TT_PRIM),
                           },
                     strlen (src9), 4);

  const char *src10 = "a,,,b;;c";
  test_scanner_case (src10, (struct token[]){
                                tt_ident (strfcstr ("a")),
                                quick_tok (TT_COMMA),
                                quick_tok (TT_COMMA),
                                quick_tok (TT_COMMA),
                                tt_ident (strfcstr ("b")),
                                quick_tok (TT_SEMICOLON),
                                quick_tok (TT_SEMICOLON),
                                tt_ident (strfcstr ("c")),
                            },
                     strlen (src10), 8);

  const char *src11 = "@";
  char *errormsg = "Scanner: Unexpected char: @";
  error expected_err = (error){
    .cause_code = ERR_SYNTAX,
    .cmlen = i_strlen (errormsg),
  };
  i_memcpy (expected_err.cause_msg, errormsg, i_strlen (errormsg));

  test_scanner_case (src11, (struct token[]){ tt_err (expected_err) }, strlen (src11), 1);

  const char *src13 = "foo_bar";
  test_scanner_case (src13, (struct token[]){ quick_tok (TT_ERROR) }, strlen (src13), 1);

  const char *src14 = "create a a u32 create b ; ,, + 123 123.4, \"fiz\", \"baz\", struct enum";
  test_scanner_case (
      src14,
      (struct token[]){
          quick_tok (TT_CREATE),
          tt_ident (strfcstr ("a")),
          tt_ident (strfcstr ("a")),
          tt_prim (U32),
          quick_tok (TT_CREATE),
          tt_ident (strfcstr ("b")),
          quick_tok (TT_SEMICOLON),
          quick_tok (TT_COMMA),
          quick_tok (TT_COMMA),
          quick_tok (TT_PLUS),
          tt_integer (123),
          tt_float (123.4f),
          quick_tok (TT_COMMA),
          tt_string (strfcstr ("fiz")),
          quick_tok (TT_COMMA),
          tt_string (strfcstr ("baz")),
          quick_tok (TT_COMMA),
          quick_tok (TT_STRUCT),
          quick_tok (TT_ENUM),
      },
      strlen (src14), 18);
}
#endif
