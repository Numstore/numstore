#include <numstore/compiler/parser/parser.h>
#include <numstore/compiler/parser/type_ref.h>
#include <numstore/types/type_ref.h>

#include <numstore/compiler/parser/subtype.h>
#include <numstore/compiler/tokens.h>
#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>

struct type_ref_parser
{
  struct parser *base;
  struct type_ref *dest;
  struct chunk_alloc temp;
  struct chunk_alloc *persistent;
};

static err_t parse_type_ref_inner (struct type_ref_parser *parser, struct type_ref *out, error *e);

/* take_type_ref ::= subtype */
static err_t
parse_take_type_ref (struct type_ref_parser *parser, struct type_ref *out, error *e)
{
  struct subtype st;
  err_t_wrap (parse_subtype (parser->base, &st, parser->persistent, e), e);

  out->type = TR_TAKE;
  out->tk.vname = st.vname;
  out->tk.ta = st.ta;

  return SUCCESS;
}

// field_ref       ::= IDENTIFIER type_ref
static inline err_t
parse_field_ref (struct kvt_ref_list_builder *builder, struct type_ref_parser *parser, error *e)
{
  // IDENT
  if (!parser_match (parser->base, TT_IDENTIFIER))
    {
      return error_causef (e, ERR_SYNTAX, "Expected identifier at position %u", parser->base->pos);
    }

  struct token *tok = parser_advance (parser->base);
  err_t_wrap (
      kvrlb_accept_key (
          builder,
          (struct string){
              .data = (char *)tok->str.data,
              .len = tok->str.len,
          },
          e),
      e);

  // Type ref
  struct type_ref inner;
  err_t_wrap (parse_type_ref_inner (parser, &inner, e), e);
  err_t_wrap (kvrlb_accept_type (builder, inner, e), e);

  return SUCCESS;
}

// struct_type_ref ::= 'struct' '{' IDENT type_ref (',' IDENT type_ref)* '}'
static err_t
parse_struct_type_ref (struct type_ref_parser *parser, struct type_ref *out, error *e)
{
  err_t err;

  // 'struct'
  err_t_wrap (parser_expect (parser->base, TT_STRUCT, e), e);

  // '{ '
  err_t_wrap (parser_expect (parser->base, TT_LEFT_BRACE, e), e);

  struct kvt_ref_list_builder builder;
  kvrlb_create (&builder, &parser->temp, parser->persistent);

  err_t_wrap (parse_field_ref (&builder, parser, e), e);

  while (parser_match (parser->base, TT_COMMA))
    {
      parser_advance (parser->base);
      err_t_wrap (parse_field_ref (&builder, parser, e), e);
    }

  err_t_wrap (parser_expect (parser->base, TT_RIGHT_BRACE, e), e);

  // Build kvt_ref list
  struct kvt_ref_list list;
  err_t_wrap (kvrlb_build (&list, &builder, e), e);

  out->type = TR_STRUCT;
  out->st = (struct struct_tr){
    .len = list.len,
    .keys = list.keys,
    .types = list.types,
  };

  return SUCCESS;
}

/* type_ref ::= struct_type_ref | take_type_ref */
static err_t
parse_type_ref_inner (struct type_ref_parser *parser, struct type_ref *out, error *e)
{
  struct token *tok = parser_peek (parser->base);

  switch (tok->type)
    {
    case TT_STRUCT:
      {
        return parse_struct_type_ref (parser, out, e);
      }
    case TT_IDENTIFIER:
      {
        return parse_take_type_ref (parser, out, e);
      }
    default:
      {
        return error_causef (
            e, ERR_SYNTAX,
            "Expected type_ref (struct or identifier) at position %u, got token type %s",
            parser->base->pos, tt_tostr (tok->type));
      }
    }
}

err_t
parse_type_ref (struct parser *p, struct type_ref *dest, struct chunk_alloc *dalloc, error *e)
{
  struct type_ref_parser parser = {
    .base = p,
    .dest = dest,
    .persistent = dalloc,
  };

  chunk_alloc_create_default (&parser.temp);

  err_t_wrap_goto (parse_type_ref_inner (&parser, parser.dest, e), theend, e);

theend:
  chunk_alloc_free_all (&parser.temp);
  return e->cause_code;
}
