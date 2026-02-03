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
 *   Implements type_accessor.h. Recursive descent parser for accessor expressions
 *   that builds a linked list of type_accessor nodes for field selection and range operations.
 */

#include <numstore/compiler/parser/type_accessor.h>
#include <numstore/compiler/lexer.h>
#include <numstore/core/string.h>

#define RANGE_NONE T_SIZE_MAX
#define T_SIZE_MAX ((t_size) ~(t_size)0)

static err_t parse_operation (struct type_accessor_parser *parser, struct type_accessor **out, error *e);

static struct type_accessor *
alloc_accessor (struct type_accessor_parser *parser, error *e)
{
  return chunk_calloc (parser->alloc, 1, sizeof (struct type_accessor), e);
}

/* Parse: select ::= '.' IDENTIFIER */
static err_t
parse_select (struct type_accessor_parser *parser, struct type_accessor **out, error *e)
{
  err_t err;

  /* Expect and consume '.' */
  err_t_wrap (parser_expect (&parser->base, TT_DOT, e), e);

  /* Expect identifier */
  if (!parser_match (&parser->base, TT_IDENTIFIER))
    {
      return error_causef (e, ERR_SYNTAX, "Expected identifier after '.' at position %u", parser->base.pos);
    }

  struct token *tok = parser_advance (&parser->base);

  /* Allocate accessor node */
  struct type_accessor *node = alloc_accessor (parser, e);
  if (!node)
    {
      return e->cause_code;
    }

  node->type = TA_SELECT;
  node->select.key.data = chunk_alloc_move_mem (parser->alloc, tok->str.data, tok->str.len, e);
  if (!node->select.key.data)
    {
      return e->cause_code;
    }
  node->select.key.len = tok->str.len;

  /* Parse continuation */
  err_t_wrap (parse_operation (parser, &node->select.sub_ta, e), e);

  *out = node;
  return SUCCESS;
}

/* Parse: range ::= '[' range_expr? ']'
 *        range_expr ::= index | slice
 *        index ::= NUMBER
 *        slice ::= start? ':' end? (':' stride?)?
 */
static err_t
parse_range (struct type_accessor_parser *parser, struct type_accessor **out, error *e)
{
  err_t err;

  /* Expect and consume '[' */
  err_t_wrap (parser_expect (&parser->base, TT_LEFT_BRACKET, e), e);

  /* Allocate accessor node */
  struct type_accessor *node = alloc_accessor (parser, e);
  if (!node)
    {
      return e->cause_code;
    }

  node->type = TA_RANGE;
  node->range.start = RANGE_NONE;
  node->range.stop = RANGE_NONE;
  node->range.step = RANGE_NONE;

  /* Check for empty brackets [] */
  if (parser_match (&parser->base, TT_RIGHT_BRACKET))
    {
      parser_advance (&parser->base);
      err_t_wrap (parse_operation (parser, &node->range.sub_ta, e), e);
      *out = node;
      return SUCCESS;
    }

  /* Check for start integer */
  if (parser_match (&parser->base, TT_INTEGER))
    {
      struct token *tok = parser_advance (&parser->base);
      node->range.start = (t_size)tok->integer;

      /* Check if this is just an index [N] */
      if (parser_match (&parser->base, TT_RIGHT_BRACKET))
        {
          parser_advance (&parser->base);
          /* For index, set stop = start + 1 to indicate single element */
          node->range.stop = node->range.start + 1;
          node->range.step = 1;
          err_t_wrap (parse_operation (parser, &node->range.sub_ta, e), e);
          *out = node;
          return SUCCESS;
        }
    }

  /* Expect first colon for slice */
  if (!parser_match (&parser->base, TT_COLON))
    {
      return error_causef (e, ERR_SYNTAX, "Expected ':' or ']' at position %u", parser->base.pos);
    }
  parser_advance (&parser->base);

  /* Check for end integer */
  if (parser_match (&parser->base, TT_INTEGER))
    {
      struct token *tok = parser_advance (&parser->base);
      node->range.stop = (t_size)tok->integer;
    }

  /* Check for second colon and stride */
  if (parser_match (&parser->base, TT_COLON))
    {
      parser_advance (&parser->base);

      /* Check for stride integer */
      if (parser_match (&parser->base, TT_INTEGER))
        {
          struct token *tok = parser_advance (&parser->base);
          node->range.step = (t_size)tok->integer;
        }
    }

  /* Expect closing bracket */
  err_t_wrap (parser_expect (&parser->base, TT_RIGHT_BRACKET, e), e);

  /* Parse continuation */
  err_t_wrap (parse_operation (parser, &node->range.sub_ta, e), e);

  *out = node;
  return SUCCESS;
}

/* Parse: operation ::= select | range | (end -> TA_TAKE) */
static err_t
parse_operation (struct type_accessor_parser *parser, struct type_accessor **out, error *e)
{
  /* Check for end of tokens */
  if (parser_at_end (&parser->base))
    {
      struct type_accessor *node = alloc_accessor (parser, e);
      if (!node)
        {
          return e->cause_code;
        }
      node->type = TA_TAKE;
      *out = node;
      return SUCCESS;
    }

  struct token *tok = parser_peek (&parser->base);

  switch (tok->type)
    {
    case TT_DOT:
      return parse_select (parser, out, e);
    case TT_LEFT_BRACKET:
      return parse_range (parser, out, e);
    default:
      {
        /* End of accessor - create TA_TAKE */
        struct type_accessor *node = alloc_accessor (parser, e);
        if (!node)
          {
            return e->cause_code;
          }
        node->type = TA_TAKE;
        *out = node;
        return SUCCESS;
      }
    }
}

/* Main entry point: Parse accessor from token stream */
err_t
parse_accessor (
    struct token *src,
    u32 src_len,
    struct type_accessor_parser *parser,
    error *e)
{
  if (!parser || !e)
    {
      return error_causef (e, ERR_INVALID_ARGUMENT, "Invalid arguments to parse_accessor");
    }

  /* Handle empty token stream */
  if (!src || src_len == 0)
    {
      struct type_accessor *node = alloc_accessor (parser, e);
      if (!node)
        {
          return e->cause_code;
        }
      node->type = TA_TAKE;
      parser->result = node;
      return SUCCESS;
    }

  /* Initialize parser state */
  parser_init (&parser->base, src, src_len);
  parser->result = NULL;

  /* Parse accessor chain */
  return parse_operation (parser, &parser->result, e);
}

/* Convenience: Parse from string */
err_t
parse_accessor_str (
    const char *path,
    u32 path_len,
    struct type_accessor **result,
    struct chunk_alloc *alloc,
    error *e)
{
  if (!result || !alloc || !e)
    {
      return error_causef (e, ERR_INVALID_ARGUMENT, "Invalid arguments to parse_accessor_str");
    }

  /* Handle empty string */
  if (!path || path_len == 0)
    {
      struct type_accessor *node = chunk_calloc (alloc, 1, sizeof (struct type_accessor), e);
      if (!node)
        {
          return e->cause_code;
        }
      node->type = TA_TAKE;
      *result = node;
      return SUCCESS;
    }

  /* Lex the path */
  struct lexer lex;
  err_t_wrap (lex_tokens (path, path_len, &lex, e), e);

  /* Parse tokens */
  struct type_accessor_parser parser;
  parser.alloc = alloc;

  err_t_wrap (parse_accessor (lex.tokens, lex.ntokens, &parser, e), e);

  *result = parser.result;
  return SUCCESS;
}

#ifndef NTEST
#include <numstore/test/testing.h>

static void
check_ta_take (struct type_accessor *ta)
{
  test_fail_if_null (ta);
  test_assert_int_equal (ta->type, TA_TAKE);
}

static void
check_ta_select (struct type_accessor *ta, const char *key)
{
  test_fail_if_null (ta);
  test_assert_int_equal (ta->type, TA_SELECT);
  test_assert_int_equal (ta->select.key.len, i_strlen (key));
  test_assert_memequal (ta->select.key.data, key, ta->select.key.len);
}

static void
check_ta_range (struct type_accessor *ta, t_size start, t_size stop, t_size step)
{
  test_fail_if_null (ta);
  test_assert_int_equal (ta->type, TA_RANGE);
  test_assert_type_equal (ta->range.start, start, t_size, PRt_size);
  test_assert_type_equal (ta->range.stop, stop, t_size, PRt_size);
  test_assert_type_equal (ta->range.step, step, t_size, PRt_size);
}

TEST (TT_UNIT, accessor_parser_empty_string)
{
  struct chunk_alloc alloc;
  chunk_alloc_create_default (&alloc);
  error e = error_create ();

  TEST_CASE ("empty string")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str ("", 0, &result, &alloc, &e), &e);
    check_ta_take (result);
  }

  TEST_CASE ("null path")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str (NULL, 0, &result, &alloc, &e), &e);
    check_ta_take (result);
  }

  chunk_alloc_free_all (&alloc);
}

TEST (TT_UNIT, accessor_parser_single_select)
{
  struct chunk_alloc alloc;
  chunk_alloc_create_default (&alloc);
  error e = error_create ();

  TEST_CASE (".field")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str (".field", 6, &result, &alloc, &e), &e);
    check_ta_select (result, "field");
    check_ta_take (result->select.sub_ta);
  }

  TEST_CASE (".x")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str (".x", 2, &result, &alloc, &e), &e);
    check_ta_select (result, "x");
    check_ta_take (result->select.sub_ta);
  }

  TEST_CASE (".longer_field_name")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str (".longer_field_name", 18, &result, &alloc, &e), &e);
    check_ta_select (result, "longer_field_name");
    check_ta_take (result->select.sub_ta);
  }

  chunk_alloc_free_all (&alloc);
}

TEST (TT_UNIT, accessor_parser_single_index)
{
  struct chunk_alloc alloc;
  chunk_alloc_create_default (&alloc);
  error e = error_create ();

  TEST_CASE ("[5]")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str ("[5]", 3, &result, &alloc, &e), &e);
    check_ta_range (result, 5, 6, 1);
    check_ta_take (result->range.sub_ta);
  }

  TEST_CASE ("[0]")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str ("[0]", 3, &result, &alloc, &e), &e);
    check_ta_range (result, 0, 1, 1);
    check_ta_take (result->range.sub_ta);
  }

  TEST_CASE ("[100]")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str ("[100]", 5, &result, &alloc, &e), &e);
    check_ta_range (result, 100, 101, 1);
    check_ta_take (result->range.sub_ta);
  }

  chunk_alloc_free_all (&alloc);
}

TEST (TT_UNIT, accessor_parser_empty_range)
{
  struct chunk_alloc alloc;
  chunk_alloc_create_default (&alloc);
  error e = error_create ();

  TEST_CASE ("[]")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str ("[]", 2, &result, &alloc, &e), &e);
    check_ta_range (result, RANGE_NONE, RANGE_NONE, RANGE_NONE);
    check_ta_take (result->range.sub_ta);
  }

  TEST_CASE ("[:]")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str ("[:]", 3, &result, &alloc, &e), &e);
    check_ta_range (result, RANGE_NONE, RANGE_NONE, RANGE_NONE);
    check_ta_take (result->range.sub_ta);
  }

  TEST_CASE ("[::]")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str ("[::]", 4, &result, &alloc, &e), &e);
    check_ta_range (result, RANGE_NONE, RANGE_NONE, RANGE_NONE);
    check_ta_take (result->range.sub_ta);
  }

  chunk_alloc_free_all (&alloc);
}

TEST (TT_UNIT, accessor_parser_slice)
{
  struct chunk_alloc alloc;
  chunk_alloc_create_default (&alloc);
  error e = error_create ();

  TEST_CASE ("[0:10]")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str ("[0:10]", 6, &result, &alloc, &e), &e);
    check_ta_range (result, 0, 10, RANGE_NONE);
    check_ta_take (result->range.sub_ta);
  }

  TEST_CASE ("[0:10:2]")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str ("[0:10:2]", 8, &result, &alloc, &e), &e);
    check_ta_range (result, 0, 10, 2);
    check_ta_take (result->range.sub_ta);
  }

  TEST_CASE ("[1:]")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str ("[1:]", 4, &result, &alloc, &e), &e);
    check_ta_range (result, 1, RANGE_NONE, RANGE_NONE);
    check_ta_take (result->range.sub_ta);
  }

  TEST_CASE ("[:5]")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str ("[:5]", 4, &result, &alloc, &e), &e);
    check_ta_range (result, RANGE_NONE, 5, RANGE_NONE);
    check_ta_take (result->range.sub_ta);
  }

  TEST_CASE ("[::3]")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str ("[::3]", 5, &result, &alloc, &e), &e);
    check_ta_range (result, RANGE_NONE, RANGE_NONE, 3);
    check_ta_take (result->range.sub_ta);
  }

  TEST_CASE ("[5::2]")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str ("[5::2]", 6, &result, &alloc, &e), &e);
    check_ta_range (result, 5, RANGE_NONE, 2);
    check_ta_take (result->range.sub_ta);
  }

  TEST_CASE ("[:10:2]")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str ("[:10:2]", 7, &result, &alloc, &e), &e);
    check_ta_range (result, RANGE_NONE, 10, 2);
    check_ta_take (result->range.sub_ta);
  }

  chunk_alloc_free_all (&alloc);
}

TEST (TT_UNIT, accessor_parser_chained)
{
  struct chunk_alloc alloc;
  chunk_alloc_create_default (&alloc);
  error e = error_create ();

  TEST_CASE (".x[0:10]")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str (".x[0:10]", 8, &result, &alloc, &e), &e);
    check_ta_select (result, "x");
    check_ta_range (result->select.sub_ta, 0, 10, RANGE_NONE);
    check_ta_take (result->select.sub_ta->range.sub_ta);
  }

  TEST_CASE ("[0:5].data")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str ("[0:5].data", 10, &result, &alloc, &e), &e);
    check_ta_range (result, 0, 5, RANGE_NONE);
    check_ta_select (result->range.sub_ta, "data");
    check_ta_take (result->range.sub_ta->select.sub_ta);
  }

  TEST_CASE ("[0:5].data[10:20]")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str ("[0:5].data[10:20]", 17, &result, &alloc, &e), &e);
    check_ta_range (result, 0, 5, RANGE_NONE);
    check_ta_select (result->range.sub_ta, "data");
    check_ta_range (result->range.sub_ta->select.sub_ta, 10, 20, RANGE_NONE);
    check_ta_take (result->range.sub_ta->select.sub_ta->range.sub_ta);
  }

  TEST_CASE (".matrix[1:10][5:15]")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str (".matrix[1:10][5:15]", 19, &result, &alloc, &e), &e);
    check_ta_select (result, "matrix");
    check_ta_range (result->select.sub_ta, 1, 10, RANGE_NONE);
    check_ta_range (result->select.sub_ta->range.sub_ta, 5, 15, RANGE_NONE);
    check_ta_take (result->select.sub_ta->range.sub_ta->range.sub_ta);
  }

  TEST_CASE (".a.b.c")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str (".a.b.c", 6, &result, &alloc, &e), &e);
    check_ta_select (result, "a");
    check_ta_select (result->select.sub_ta, "b");
    check_ta_select (result->select.sub_ta->select.sub_ta, "c");
    check_ta_take (result->select.sub_ta->select.sub_ta->select.sub_ta);
  }

  TEST_CASE ("[0][1][2]")
  {
    struct type_accessor *result;
    test_err_t_wrap (parse_accessor_str ("[0][1][2]", 9, &result, &alloc, &e), &e);
    check_ta_range (result, 0, 1, 1);
    check_ta_range (result->range.sub_ta, 1, 2, 1);
    check_ta_range (result->range.sub_ta->range.sub_ta, 2, 3, 1);
    check_ta_take (result->range.sub_ta->range.sub_ta->range.sub_ta);
  }

  chunk_alloc_free_all (&alloc);
}

TEST (TT_UNIT, accessor_parser_from_tokens)
{
  struct chunk_alloc alloc;
  chunk_alloc_create_default (&alloc);
  error e = error_create ();

  TEST_CASE ("parse from token array")
  {
    struct token tokens[] = {
      quick_tok (TT_DOT),
      tt_ident ("field", 5),
      quick_tok (TT_LEFT_BRACKET),
      tt_integer (0),
      quick_tok (TT_COLON),
      tt_integer (10),
      quick_tok (TT_RIGHT_BRACKET),
    };

    struct type_accessor_parser parser;
    parser.alloc = &alloc;

    test_err_t_wrap (parse_accessor (tokens, arrlen (tokens), &parser, &e), &e);

    check_ta_select (parser.result, "field");
    check_ta_range (parser.result->select.sub_ta, 0, 10, RANGE_NONE);
    check_ta_take (parser.result->select.sub_ta->range.sub_ta);
  }

  TEST_CASE ("empty token array")
  {
    struct type_accessor_parser parser;
    parser.alloc = &alloc;

    test_err_t_wrap (parse_accessor (NULL, 0, &parser, &e), &e);
    check_ta_take (parser.result);
  }

  chunk_alloc_free_all (&alloc);
}

TEST (TT_UNIT, accessor_parser_lexer_dot)
{
  error e = error_create ();

  TEST_CASE ("dot followed by identifier")
  {
    struct lexer lex;
    test_err_t_wrap (lex_tokens (".field", 6, &lex, &e), &e);
    test_assert_int_equal (lex.ntokens, 2);
    test_assert_int_equal (lex.tokens[0].type, TT_DOT);
    test_assert_int_equal (lex.tokens[1].type, TT_IDENTIFIER);
  }

  TEST_CASE ("dot followed by number is float")
  {
    struct lexer lex;
    test_err_t_wrap (lex_tokens (".5", 2, &lex, &e), &e);
    test_assert_int_equal (lex.ntokens, 1);
    test_assert_int_equal (lex.tokens[0].type, TT_FLOAT);
  }

  TEST_CASE ("multiple dots")
  {
    struct lexer lex;
    test_err_t_wrap (lex_tokens (".a.b", 4, &lex, &e), &e);
    test_assert_int_equal (lex.ntokens, 4);
    test_assert_int_equal (lex.tokens[0].type, TT_DOT);
    test_assert_int_equal (lex.tokens[1].type, TT_IDENTIFIER);
    test_assert_int_equal (lex.tokens[2].type, TT_DOT);
    test_assert_int_equal (lex.tokens[3].type, TT_IDENTIFIER);
  }
}

#endif /* NTEST */
