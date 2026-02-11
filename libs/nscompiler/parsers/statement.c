#include "numstore/types/subtype.h"
#include <numstore/compiler/parser/parser.h>
#include <numstore/compiler/parser/statement.h>
#include <numstore/compiler/parser/stride.h>
#include <numstore/compiler/parser/type.h>
#include <numstore/compiler/parser/type_ref.h>
#include <numstore/compiler/tokens.h>
#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/types/statement.h>

struct statement_parser
{
  struct parser *base;
  struct statement *dest;
  struct chunk_alloc temp;
  struct chunk_alloc *persistent;
};

/* create_cmd ::= 'create' VNAME type */
static err_t
parse_create_cmd (struct statement_parser *parser, error *e)
{
  err_t_wrap (parser_expect (parser->base, TT_CREATE, e), e);

  if (!parser_match (parser->base, TT_IDENTIFIER))
    {
      return error_causef (e, ERR_SYNTAX,
                           "Expected variable name at position %u",
                           parser->base->pos);
    }

  // VNAME
  struct token *vname_tok = parser_advance (parser->base);
  struct string vname = {
    .data = vname_tok->str.data,
    .len = vname_tok->str.len
  };

  // TYPE
  struct type vtype;
  err_t_wrap (parse_type (parser->base, &vtype, parser->persistent, e), e);

  parser->dest->type = ST_CREATE;
  return crtst_create (parser->dest, vname, vtype, e);
}

/* delete_cmd ::= 'delete' VNAME */
static err_t
parse_delete_cmd (struct statement_parser *parser, error *e)
{
  err_t_wrap (parser_expect (parser->base, TT_DELETE, e), e);

  if (!parser_match (parser->base, TT_IDENTIFIER))
    {
      return error_causef (e, ERR_SYNTAX,
                           "Expected variable name at position %u",
                           parser->base->pos);
    }

  // VNAME
  struct token *vname_tok = parser_advance (parser->base);
  struct string vname = {
    .data = vname_tok->str.data,
    .len = vname_tok->str.len
  };

  parser->dest->type = ST_DELETE;
  return dltst_create (parser->dest, vname, e);
}

/* insert_cmd ::= 'insert' VNAME ('OFST' NUMBER)? ('LEN' NUMBER)? */
static err_t
parse_insert_cmd (struct statement_parser *parser, error *e)
{
  err_t_wrap (parser_expect (parser->base, TT_INSERT, e), e);

  // VNAME
  if (!parser_match (parser->base, TT_IDENTIFIER))
    {
      return error_causef (e, ERR_SYNTAX,
                           "Expected variable name at position %u",
                           parser->base->pos);
    }

  struct token *vname_tok = parser_advance (parser->base);
  struct string vname = {
    .data = vname_tok->str.data,
    .len = vname_tok->str.len
  };

  // Optional OFST
  sb_size ofst = -1;
  if (parser_match (parser->base, TT_OFST))
    {
      parser_advance (parser->base);

      if (!parser_match (parser->base, TT_INTEGER))
        {
          return error_causef (e, ERR_SYNTAX,
                               "Expected number after OFST at position %u",
                               parser->base->pos);
        }

      struct token *ofst_tok = parser_advance (parser->base);
      ofst = ofst_tok->integer;
    }

  // Optional LEN
  sb_size nelems = -1;
  if (parser_match (parser->base, TT_LEN))
    {
      parser_advance (parser->base);

      if (!parser_match (parser->base, TT_INTEGER))
        {
          return error_causef (e, ERR_SYNTAX,
                               "Expected number after LEN at position %u",
                               parser->base->pos);
        }

      struct token *nelems_tok = parser_advance (parser->base);
      nelems = nelems_tok->integer;
    }

  parser->dest->type = ST_INSERT;
  return insst_create (parser->dest, vname, ofst, nelems, e);
}

/* read_cmd ::= 'read' type_ref */
static err_t
parse_read_cmd (struct statement_parser *parser, error *e)
{
  err_t_wrap (parser_expect (parser->base, TT_READ, e), e);

  struct type_ref tr;
  err_t_wrap (parse_type_ref (parser->base, &tr, parser->persistent, e), e);

  // Default stride (all elements)
  struct user_stride stride = USER_STRIDE_ALL;

  parser->dest->type = ST_READ;
  return redst_create (parser->dest, tr, stride, e);
}

/* remove_cmd ::= 'remove' VNAME slice */
static err_t
parse_remove_cmd (struct statement_parser *parser, error *e)
{
  err_t_wrap (parser_expect (parser->base, TT_REMOVE, e), e);

  if (!parser_match (parser->base, TT_IDENTIFIER))
    {
      return error_causef (e, ERR_SYNTAX,
                           "Expected variable name at position %u",
                           parser->base->pos);
    }

  // VNAME
  struct token *vname_tok = parser_advance (parser->base);
  struct string vname = {
    .data = vname_tok->str.data,
    .len = vname_tok->str.len
  };

  // Create a TAKE type_ref for the variable
  struct type_accessor ta = {
    .type = TA_TAKE
  };

  struct type_ref tr = {
    .type = TR_TAKE,
    .tk = {
        .vname = vname,
        .ta = ta }
  };

  // STRIDE (required)
  struct user_stride stride;
  err_t_wrap (parse_stride (parser->base, &stride, e), e);

  parser->dest->type = ST_REMOVE;
  return remst_create (parser->dest, tr, stride, e);
}

/* command ::= create_cmd | delete_cmd | insert_cmd | read_cmd | remove_cmd */
static err_t
parse_statement_inner (struct statement_parser *parser, error *e)
{
  struct token *tok = parser_peek (parser->base);

  switch (tok->type)
    {
    case TT_CREATE:
      {
        return parse_create_cmd (parser, e);
      }

    case TT_DELETE:
      {
        return parse_delete_cmd (parser, e);
      }

    case TT_INSERT:
      {
        return parse_insert_cmd (parser, e);
      }

    case TT_READ:
      {
        return parse_read_cmd (parser, e);
      }

    case TT_REMOVE:
      {
        return parse_remove_cmd (parser, e);
      }

    default:
      {
        return error_causef (
            e, ERR_SYNTAX,
            "Expected command at position %u, got token type %s",
            parser->base->pos, tt_tostr (tok->type));
      }
    }
}

err_t
parse_statement (struct parser *p, struct statement *dest, struct chunk_alloc *dalloc, error *e)
{
  struct statement_parser parser = {
    .base = p,
    .dest = dest,
    .persistent = dalloc,
  };

  chunk_alloc_create_default (&parser.temp);

  err_t_wrap_goto (parse_statement_inner (&parser, e), theend, e);

theend:
  chunk_alloc_free_all (&parser.temp);
  return e->cause_code;
}
