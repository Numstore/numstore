#include "numstore/core/chunk_alloc.h"
#include "numstore/core/error.h"
#include "numstore/types/enum.h"
#include "numstore/types/kvt_builder.h"
#include "numstore/types/sarray.h"
#include "numstore/types/types.h"
#include <numstore/compiler/parser/type.h>

#include <stdlib.h>
#include <string.h>

/* Helper functions */
static inline struct token *
peek (struct type_parser *parser)
{
  return (parser->pos < parser->src_len) ? &parser->src[parser->pos] : NULL;
}

static inline bool
match (const struct token *tok, enum token_t type)
{
  return tok && tok->type == type;
}

static err_t
expect (struct type_parser *parser, enum token_t type, error *e)
{
  const struct token *tok = peek (parser);
  if (!match (tok, type))
    {
      return error_causef (e, ERR_SYNTAX, "Expected token type %d at position %u, got %d", type, parser->pos, tok ? (int)tok->type : -1);
    }
  parser->pos++;
  return SUCCESS;
}

/* primitive_type ::= IDENTIFIER */
static err_t
parse_primitive_type (struct type_parser *parser, struct type *out, error *e)
{
  const struct token *tok = peek (parser);

  if (!match (tok, TT_PRIM))
    {
      return error_causef (e, ERR_SYNTAX, "Expected primitive type at position %u", parser->pos);
    }

  out->type = T_PRIM;
  out->p = tok->prim;
  parser->pos++;

  return SUCCESS;
}

/* Forward declare only parse_type_inner since it's mutually recursive */
static err_t parse_type_inner (struct type_parser *parser, struct type *out, error *e);

/* sarray_type ::= '[' NUMBER ']' type */
static err_t
parse_sarray_type (struct type_parser *parser, struct type *out, error *e)
{
  err_t err;

  err_t_wrap (expect (parser, TT_LEFT_BRACKET, e), e);

  const struct token *tok = peek (parser);
  if (!match (tok, TT_INTEGER) || tok->integer < 0)
    {
      return error_causef (e, ERR_SYNTAX, "Expected array size at position %u", parser->pos);
    }

  u32 size = tok->integer;
  parser->pos++;

  err_t_wrap (expect (parser, TT_RIGHT_BRACKET, e), e);

  /* Parse element type */
  struct type inner;
  err_t_wrap (parse_type_inner (parser, &inner, e), e);

  /* Build array */
  struct sarray_builder builder;
  sab_create (&builder, &parser->temp, &parser->alloc);
  err_t_wrap (sab_accept_dim (&builder, size, e), e);
  err_t_wrap (sab_accept_type (&builder, inner, e), e);

  out->type = T_SARRAY;
  return sab_build (&out->sa, &builder, e);
}

/* enum_type ::= 'enum' '{' enum_list '}' */
static err_t
parse_enum_type (struct type_parser *parser, struct type *out, error *e)
{
  err_t err;

  err_t_wrap (expect (parser, TT_ENUM, e), e);
  err_t_wrap (expect (parser, TT_LEFT_BRACE, e), e);

  struct enum_builder builder;
  enb_create (&builder, &parser->temp, &parser->alloc);

  while (true)
    {
      struct token *tok = peek (parser);

      /* Check for closing brace (empty enum or after trailing comma) */
      if (match (tok, TT_RIGHT_BRACE))
        {
          parser->pos++;
          break;
        }

      /* Parse enum value */
      if (!match (tok, TT_IDENTIFIER))
        {
          return error_causef (e, ERR_SYNTAX, "Expected enum value at position %u", parser->pos);
        }

      char *data = (char *)tok->str.data;
      err_t_wrap (enb_accept_key (&builder, (struct string){ .data = data, .len = tok->str.len }, e), e);
      parser->pos++;

      tok = peek (parser);

      // Check for closing brace
      if (match (tok, TT_RIGHT_BRACE))
        {
          parser->pos++;
          break;
        }

      // Expect comma
      else if (match (tok, TT_COMMA))
        {
          parser->pos++;
          continue;
        }

      // Invalid token
      else
        {
          return error_causef (e, ERR_SYNTAX, "Expected ',' or '}' at position %u", parser->pos);
        }
    }

  out->type = T_ENUM;
  return enb_build (&out->en, &builder, e);
}

/* struct_type ::= 'struct' '{' field_list '}' */
static err_t
parse_struct_type (struct type_parser *parser, struct type *out, error *e)
{
  err_t err;

  err_t_wrap (expect (parser, TT_STRUCT, e), e);
  err_t_wrap (expect (parser, TT_LEFT_BRACE, e), e);

  struct kvt_builder builder;
  kvb_create (&builder, &parser->temp, &parser->alloc);

  while (true)
    {
      struct token *tok = peek (parser);

      /* Check for closing brace (empty struct or after trailing comma) */
      if (match (tok, TT_RIGHT_BRACE))
        {
          parser->pos++;
          break;
        }

      /* Parse field name */
      if (!match (tok, TT_IDENTIFIER))
        {
          return error_causef (e, ERR_SYNTAX, "Expected field name at position %u", parser->pos);
        }

      char *data = (char *)tok->str.data;
      err_t_wrap (kvb_accept_key (&builder, (struct string){ .data = data, .len = tok->str.len }, e), e);
      parser->pos++;

      /* Parse field type */
      struct type inner;
      err_t_wrap (parse_type_inner (parser, &inner, e), e);
      err_t_wrap (kvb_accept_type (&builder, inner, e), e);

      tok = peek (parser);

      // Check for closing brace
      if (match (tok, TT_RIGHT_BRACE))
        {
          parser->pos++;
          break;
        }

      // Expect comma
      if (match (tok, TT_COMMA))
        {
          parser->pos++;
          continue;
        }

      // Invalid token
      else
        {
          return error_causef (e, ERR_SYNTAX, "Expected ',' or '}' at position %u", parser->pos);
        }
    }

  out->type = T_STRUCT;
  return kvb_struct_t_build (&out->st, &builder, e);
}

/* union_type ::= 'union' '{' field_list '}' */
static err_t
parse_union_type (struct type_parser *parser, struct type *out, error *e)
{
  err_t err;

  err_t_wrap (expect (parser, TT_UNION, e), e);
  err_t_wrap (expect (parser, TT_LEFT_BRACE, e), e);

  struct kvt_builder builder;
  kvb_create (&builder, &parser->temp, &parser->alloc);

  while (true)
    {
      struct token *tok = peek (parser);

      /* Check for closing brace (empty union or after trailing comma) */
      if (match (tok, TT_RIGHT_BRACE))
        {
          parser->pos++;
          break;
        }

      /* Parse field name */
      if (!match (tok, TT_IDENTIFIER))
        {
          return error_causef (e, ERR_SYNTAX, "Expected field name at position %u", parser->pos);
        }

      char *data = (char *)tok->str.data;
      err_t_wrap (kvb_accept_key (&builder, (struct string){ .data = data, .len = tok->str.len }, e), e);
      parser->pos++;

      /* Parse field type */
      struct type inner;
      err_t_wrap (parse_type_inner (parser, &inner, e), e);
      err_t_wrap (kvb_accept_type (&builder, inner, e), e);

      tok = peek (parser);

      // Check for closing brace
      if (match (tok, TT_RIGHT_BRACE))
        {
          parser->pos++;
          break;
        }

      // Expect comma
      if (match (tok, TT_COMMA))
        {
          parser->pos++;
          continue;
        }

      // Invalid token
      else
        {
          return error_causef (e, ERR_SYNTAX, "Expected ',' or '}' at position %u", parser->pos);
        }
    }

  out->type = T_UNION;
  return kvb_union_t_build (&out->un, &builder, e);
}

/* type ::= struct_type | union_type | enum_type | sarray_type | primitive_type */
static err_t
parse_type_inner (struct type_parser *parser, struct type *out, error *e)
{
  const struct token *tok = peek (parser);

  if (!tok)
    {
      return error_causef (e, ERR_SYNTAX, "Unexpected end of input at position %u", parser->pos);
    }

  switch (tok->type)
    {
    case TT_STRUCT:
      {
        return parse_struct_type (parser, out, e);
      }
    case TT_UNION:
      {
        return parse_union_type (parser, out, e);
      }
    case TT_ENUM:
      {
        return parse_enum_type (parser, out, e);
      }
    case TT_LEFT_BRACKET:
      {
        return parse_sarray_type (parser, out, e);
      }
    case TT_PRIM:
      {
        return parse_primitive_type (parser, out, e);
      }
    default:
      {
        return error_causef (e, ERR_SYNTAX, "Expected type at position %u, got token type %d", parser->pos, tok->type);
      }
    }
}

/* Main entry point */
err_t
parse_type (struct token *src, u32 src_len, struct type_parser *parser, error *e)
{

  if (!src || !parser || !e)
    {
      return error_causef (e, ERR_INVALID_ARGUMENT, "Invalid arguments to parse_type");
    }

  /* Initialize parser state */
  parser->src = src;
  parser->src_len = src_len;
  parser->pos = 0;
  memset (&parser->dest, 0, sizeof (parser->dest));

  chunk_alloc_create_default (&parser->temp);
  chunk_alloc_create_default (&parser->alloc);

  /* Parse type */
  err_t_wrap (parse_type_inner (parser, &parser->dest, e), e);

  /* Check for EOF */
  if (parser->pos >= parser->src_len)
    {
      chunk_alloc_free_all (&parser->temp);
      chunk_alloc_free_all (&parser->alloc);
      return error_causef (e, ERR_SYNTAX, "Unexpected tokens after type declaration at position %u", parser->pos);
    }

  chunk_alloc_free_all (&parser->temp);

  return SUCCESS;
}
