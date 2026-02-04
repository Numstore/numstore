#include "numstore/types/subtype.h"
#include "numstore/compiler/parser/parser.h"
#include "numstore/compiler/tokens.h"
#include <numstore/compiler/parser/subtype.h>

#include <numstore/compiler/parser/stride.h>
#include <numstore/core/assert.h>
#include <numstore/intf/logging.h>
#include <numstore/test/testing.h>

struct sub_type_parser
{
  struct parser *base;
  struct subtype *dest;
  struct chunk_alloc temp;
  struct chunk_alloc *persistent;
};

/*
 * sub_type   ::= IDENT stride* ('.' IDENT stride*)*
 */
static err_t
parse_sub_type_inner (struct sub_type_parser *parser, error *e)
{
  if (!parser_match (parser->base, TT_IDENTIFIER))
    {
      return error_causef (e, ERR_SYNTAX,
                           "Expected variable name at position %u",
                           parser->base->pos);
    }

  // VNAME
  struct token *tok = parser_advance (parser->base);
  struct string vname = { .data = tok->str.data, .len = tok->str.len };

  // Type accessors
  struct type_accessor_builder tab;
  tab_create (&tab, parser->persistent);
  while (true)
    {
      // Stride
      if (parser_match (parser->base, TT_LEFT_BRACKET))
        {
          struct user_stride stride;
          err_t_wrap (parse_stride (parser->base, &stride, e), e);
          err_t_wrap (tab_accept_range (&tab, stride, e), e);
        }

      // Dot
      else if (parser_match (parser->base, TT_DOT))
        {
          parser_advance (parser->base);
          err_t_wrap (parser_expect (parser->base, TT_IDENTIFIER, e), e);
          tok = parser_advance (parser->base);
          struct string select = {
            .data = tok->str.data,
            .len = tok->str.len,
          };
          err_t_wrap (tab_accept_select (&tab, select, e), e);
        }

      // Done
      else
        {
          break;
        }
    }

  struct type_accessor ta;
  err_t_wrap (tab_build (&ta, &tab, e), e);

  return subtype_create (parser->dest, vname, ta, e);
}

err_t
parse_subtype (struct parser *p, struct subtype *dest, struct chunk_alloc *dalloc, error *e)
{
  struct sub_type_parser parser = {
    .base = p,
    .dest = dest,
    .persistent = dalloc,
  };

  chunk_alloc_create_default (&parser.temp);

  err_t rc = parse_sub_type_inner (&parser, e);

  chunk_alloc_free_all (&parser.temp);

  return rc;
}
