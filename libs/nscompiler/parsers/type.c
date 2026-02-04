#include <numstore/types/types.h>

#include <numstore/compiler/parser/type.h>
#include <numstore/compiler/tokens.h>
#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>

struct type_parser
{
  struct parser *base;
  struct type *dest;
  struct chunk_alloc temp;
  struct chunk_alloc *persistent;
};

static err_t parse_type_inner (struct type_parser *parser, struct type *out, error *e);

/* primitive_type ::= PRIM */
static err_t
parse_primitive_type (struct type_parser *parser, struct type *out, error *e)
{
  if (!parser_match (parser->base, TT_PRIM))
    {
      return error_causef (e, ERR_SYNTAX, "Expected primitive type at position %u", parser->base->pos);
    }

  struct token *tok = parser_advance (parser->base);
  out->type = T_PRIM;
  out->p = tok->prim;

  return SUCCESS;
}

/* sarray_type ::= '[' INTEGER ']'+ type */
static err_t
parse_sarray_type (struct type_parser *parser, struct type *out, error *e)
{
  err_t err;

  struct sarray_builder builder;
  sab_create (&builder, &parser->temp, parser->persistent);

  // Parse all [N] brackets
  while (parser_match (parser->base, TT_LEFT_BRACKET))
    {
      err_t_wrap (parser_expect (parser->base, TT_LEFT_BRACKET, e), e);

      if (!parser_match (parser->base, TT_INTEGER))
        {
          return error_causef (e, ERR_SYNTAX, "Expected array size at position %u", parser->base->pos);
        }

      struct token *tok = parser_advance (parser->base);

      err_t_wrap (sab_accept_dim (&builder, tok->integer, e), e);
      err_t_wrap (parser_expect (parser->base, TT_RIGHT_BRACKET, e), e);
    }

  // Inner most type
  struct type inner;
  err_t_wrap (parse_type_inner (parser, &inner, e), e);
  err_t_wrap (sab_accept_type (&builder, inner, e), e);

  out->type = T_SARRAY;
  return sab_build (&out->sa, &builder, e);
}

/* enum_type ::= 'enum' '{' enum_list? '}' */
static err_t
parse_enum_type (struct type_parser *parser, struct type *out, error *e)
{
  err_t err;

  err_t_wrap (parser_expect (parser->base, TT_ENUM, e), e);
  err_t_wrap (parser_expect (parser->base, TT_LEFT_BRACE, e), e);

  struct enum_builder builder;
  enb_create (&builder, &parser->temp, parser->persistent);

  while (!parser_match (parser->base, TT_RIGHT_BRACE))
    {
      // Enum value
      if (!parser_match (parser->base, TT_IDENTIFIER))
        {
          return error_causef (e, ERR_SYNTAX, "Expected identifier at position %u", parser->base->pos);
        }

      struct token *tok = parser_advance (parser->base);
      err_t_wrap (
          enb_accept_key (
              &builder,
              (struct string){
                  .data = (char *)tok->str.data,
                  .len = tok->str.len,
              },
              e),
          e);
    }

  err_t_wrap (parser_expect (parser->base, TT_RIGHT_BRACE, e), e);

  out->type = T_ENUM;

  return enb_build (&out->en, &builder, e);
}

/* struct_type ::= 'struct' '{' field_list? '}' */
static err_t
parse_struct_type (struct type_parser *parser, struct type *out, error *e)
{
  err_t err;

  // 'struct'
  err_t_wrap (parser_expect (parser->base, TT_UNION, e), e);

  // '{ '
  err_t_wrap (parser_expect (parser->base, TT_LEFT_BRACE, e), e);

  struct kvt_list_builder builder;
  kvlb_create (&builder, &parser->temp, parser->persistent);

  while (!parser_match (parser->base, TT_RIGHT_BRACE))
    {
      // Key
      if (!parser_match (parser->base, TT_IDENTIFIER))
        {
          return error_causef (e, ERR_SYNTAX, "Expected field name at position %u", parser->base->pos);
        }

      struct token *tok = parser_advance (parser->base);
      err_t_wrap (
          kvlb_accept_key (
              &builder,
              (struct string){
                  .data = tok->str.data,
                  .len = tok->str.len,
              },
              e),
          e);

      // Type
      struct type inner;
      err_t_wrap (parse_type_inner (parser, &inner, e), e);
      err_t_wrap (kvlb_accept_type (&builder, inner, e), e);
    }

  err_t_wrap (parser_expect (parser->base, TT_RIGHT_BRACE, e), e);

  out->type = T_UNION;

  // Build kvt list
  struct kvt_list list;
  err_t_wrap (kvlb_build (&list, &builder, e), e);

  return struct_t_create (&out->st, list, NULL, e);
}

/* union_type ::= 'union' '{' field_list? '}' */
static err_t
parse_union_type (struct type_parser *parser, struct type *out, error *e)
{
  err_t err;

  // 'union'
  err_t_wrap (parser_expect (parser->base, TT_UNION, e), e);

  // '{ '
  err_t_wrap (parser_expect (parser->base, TT_LEFT_BRACE, e), e);

  struct kvt_list_builder builder;
  kvlb_create (&builder, &parser->temp, parser->persistent);

  while (!parser_match (parser->base, TT_RIGHT_BRACE))
    {
      // Key
      if (!parser_match (parser->base, TT_IDENTIFIER))
        {
          return error_causef (e, ERR_SYNTAX, "Expected field name at position %u", parser->base->pos);
        }

      struct token *tok = parser_advance (parser->base);
      err_t_wrap (
          kvlb_accept_key (
              &builder,
              (struct string){
                  .data = tok->str.data,
                  .len = tok->str.len,
              },
              e),
          e);

      // Type
      struct type inner;
      err_t_wrap (parse_type_inner (parser, &inner, e), e);
      err_t_wrap (kvlb_accept_type (&builder, inner, e), e);
    }

  err_t_wrap (parser_expect (parser->base, TT_RIGHT_BRACE, e), e);

  out->type = T_UNION;

  // Build kvt list
  struct kvt_list list;
  err_t_wrap (kvlb_build (&list, &builder, e), e);

  return union_t_create (&out->un, list, NULL, e);
}

/* type ::= struct_type | union_type | enum_type | sarray_type | primitive_type */
static err_t
parse_type_inner (struct type_parser *parser, struct type *out, error *e)
{
  struct token *tok = parser_peek (parser->base);

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
        return error_causef (
            e, ERR_SYNTAX,
            "Expected type at position %u, got token type %s",
            parser->base->pos, tt_tostr (tok->type));
      }
    }
}

err_t
parse_type (struct parser *p, struct type *dest, struct chunk_alloc *dalloc, error *e)
{
  struct type_parser parser = {
    .base = p,
    .dest = dest,
    .persistent = dalloc,
  };

  chunk_alloc_create_default (&parser.temp);

  err_t_wrap_goto (parse_type_inner (&parser, parser.dest, e), theend, e);

theend:
  chunk_alloc_free_all (&parser.temp);
  return e->cause_code;
}
