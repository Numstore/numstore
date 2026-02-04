#include "numstore/types/subtype.h"
#include <numstore/compiler/parser/parser.h>
#include <numstore/compiler/parser/statement.h>
#include <numstore/compiler/parser/stride.h>
#include <numstore/compiler/parser/subtype.h>
#include <numstore/compiler/parser/type.h>
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

/* insert_cmd ::= 'insert' VNAME NUMBER NUMBER */
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

  // OFFSET
  if (!parser_match (parser->base, TT_INTEGER))
    {
      return error_causef (e, ERR_SYNTAX,
                           "Expected offset at position %u",
                           parser->base->pos);
    }

  struct token *ofst_tok = parser_advance (parser->base);
  b_size ofst = ofst_tok->integer;

  // NELEMS
  if (!parser_match (parser->base, TT_INTEGER))
    {
      return error_causef (e, ERR_SYNTAX,
                           "Expected number of elements at position %u",
                           parser->base->pos);
    }

  struct token *nelems_tok = parser_advance (parser->base);
  b_size nelems = nelems_tok->integer;

  parser->dest->type = ST_INSERT;
  return insst_create (parser->dest, vname, ofst, nelems, e);
}

/* append_cmd ::= 'append' VNAME NUMBER */
static err_t
parse_append_cmd (struct statement_parser *parser, error *e)
{
  err_t_wrap (parser_expect (parser->base, TT_APPEND, e), e);

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

  // NELEMS
  if (!parser_match (parser->base, TT_INTEGER))
    {
      return error_causef (e, ERR_SYNTAX,
                           "Expected number of elements at position %u",
                           parser->base->pos);
    }

  struct token *nelems_tok = parser_advance (parser->base);
  b_size nelems = nelems_tok->integer;

  parser->dest->type = ST_APPEND;

  return appst_create (parser->dest, vname, nelems, e);
}

/* var_ref ::= IDENT ('as' IDENT)? */
static err_t
parse_var_ref (
    struct statement_parser *parser,
    struct vref_list *dest,
    error *e)
{
  struct vref_list_builder builder;
  vrlb_create (&builder, &parser->temp, parser->persistent);

  if (!parser_match (parser->base, TT_IDENTIFIER))
    {
      return error_causef (e, ERR_SYNTAX,
                           "Expected identifier at position %u",
                           parser->base->pos);
    }

  struct token *name_tok = parser_advance (parser->base);
  const char *name = name_tok->str.data;
  const char *ref = name;

  if (parser_match (parser->base, TT_AS))
    {
      parser_advance (parser->base);

      if (!parser_match (parser->base, TT_IDENTIFIER))
        {
          return error_causef (e, ERR_SYNTAX, "Expected identifier after 'as' at position %u", parser->base->pos);
        }

      struct token *ref_tok = parser_advance (parser->base);
      ref = ref_tok->str.data;
    }

  err_t_wrap (vrlb_accept (&builder, name, ref, e), e);

  return vrlb_build (dest, &builder, e);
}

/* field_selection ::= '[' type_accessor (',' type_accessor)* ']' */
static err_t
parse_field_selection (
    struct statement_parser *parser,
    struct subtype_list *dest,
    error *e)
{
  struct subtype_list_builder builder;
  stalb_create (&builder, &parser->temp, parser->persistent);

  err_t_wrap (parser_expect (parser->base, TT_LEFT_BRACKET, e), e);

  while (true)
    {
      struct subtype ta;
      if (parse_subtype (parser->base, &ta, parser->persistent, e))
        {
          return e->cause_code;
        }

      err_t_wrap (stalb_accept (&builder, ta, e), e);

      if (parser_match (parser->base, TT_RIGHT_BRACKET))
        {
          break;
        }
      else if (parser_match (parser->base, TT_COMMA))
        {
          parser_advance (parser->base);
        }
      else
        {
          return error_causef (e, ERR_SYNTAX, "Expected ',' or ']' at position %u", parser->base->pos);
        }
    }

  err_t_wrap (parser_expect (parser->base, TT_RIGHT_BRACKET, e), e);

  return stalb_build (dest, &builder, e);
}

/* var_selection ::= var_ref (',' var_ref)* field_selection? slice? */
static err_t
parse_var_selection (
    struct statement_parser *parser,
    struct vref_list *vdest,
    struct subtype_list *tdest,
    struct user_stride *gstride,
    bool *has_ta_list,
    bool *has_gstride,
    error *e)
{
  // Parse var_ref list
  err_t_wrap (parse_var_ref (parser, vdest, e), e);

  while (parser_match (parser->base, TT_COMMA))
    {
      parser_advance (parser->base);
      err_t_wrap (parse_var_ref (parser, vdest, e), e);
    }

  // Look ahead of 2 for [ident instead of [number/colon
  if (parser_match (parser->base, TT_LEFT_BRACKET) && parser_peek (parser->base)->type == TT_IDENTIFIER)
    {
      err_t_wrap (parse_field_selection (parser, tdest, e), e);
      *has_ta_list = true;
    }

  // Optional slice
  if (parser_match (parser->base, TT_LEFT_BRACKET))
    {
      err_t_wrap (parse_stride (parser->base, gstride, e), e);
      *has_gstride = true;
    }
  else
    {
      *has_gstride = false;
    }

  return SUCCESS;
}

/* read_cmd ::= 'read' var_selection */
static err_t
parse_read_cmd (struct statement_parser *parser, error *e)
{
  err_t_wrap (parser_expect (parser->base, TT_READ, e), e);

  struct vref_list vlist;
  struct subtype_list talist;
  struct user_stride stride;
  bool has_ta_list;
  bool has_gstride;

  err_t_wrap (parse_var_selection (parser, &vlist, &talist, &stride, &has_ta_list, &has_gstride, e), e);

  if (has_ta_list)
    {
      // err_t_wrap (rdb_accept_accessor_list (&builder, talist, e), e);
    }

  if (has_gstride)
    {
      // err_t_wrap (rdb_accept_stride (&builder, stride, e), e);
    }

  parser->dest->type = ST_READ;
  return redst_create (parser->dest, vlist, talist, stride, e);
}

/* write_cmd ::= 'write' var_selection */
static err_t
parse_write_cmd (struct statement_parser *parser, error *e)
{
  err_t_wrap (parser_expect (parser->base, TT_WRITE, e), e);

  struct vref_list vlist;
  struct subtype_list talist;
  struct user_stride stride;
  bool has_ta_list;
  bool has_gstride;

  err_t_wrap (parse_var_selection (parser, &vlist, &talist, &stride, &has_ta_list, &has_gstride, e), e);

  if (has_ta_list)
    {
      // err_t_wrap (wrb_accept_accessor_list (&builder, talist, e), e);
    }

  if (has_gstride)
    {
      // err_t_wrap (wrb_accept_stride (&builder, stride, e), e);
    }

  parser->dest->type = ST_WRITE;
  return wrtst_create (parser->dest, vlist, talist, stride, e);
}

/* take_cmd ::= 'take' var_selection */
static err_t
parse_take_cmd (struct statement_parser *parser, error *e)
{
  err_t_wrap (parser_expect (parser->base, TT_TAKE, e), e);

  struct vref_list vlist;
  struct subtype_list talist;
  struct user_stride stride;
  bool has_ta_list;
  bool has_gstride;

  err_t_wrap (parse_var_selection (parser, &vlist, &talist, &stride, &has_ta_list, &has_gstride, e), e);

  if (has_ta_list)
    {
      // err_t_wrap (tkb_accept_accessor_list (&builder, talist, e), e);
    }

  if (has_gstride)
    {
      // err_t_wrap (tkb_accept_stride (&builder, stride, e), e);
    }

  parser->dest->type = ST_TAKE;
  return takst_create (parser->dest, vlist, talist, stride, e);
}

/* remove_cmd ::= 'remove' VNAME slice? */
static err_t
parse_remove_cmd (struct statement_parser *parser, error *e)
{
  err_t_wrap (parser_expect (parser->base, TT_REMOVE, e), e);

  if (!parser_match (parser->base, TT_IDENTIFIER))
    {
      return error_causef (e, ERR_SYNTAX, "Expected variable name at position %u", parser->base->pos);
    }

  // VNAME
  struct token *name_tok = parser_advance (parser->base);
  struct vref ref = {
    .vname = { .data = name_tok->str.data, .len = name_tok->str.len },
    .alias = { .data = name_tok->str.data, .len = name_tok->str.len }
  };

  // STRIDE
  struct user_stride gstride;
  bool has_stride = false;
  if (parser_match (parser->base, TT_LEFT_BRACKET))
    {
      err_t_wrap (parse_stride (parser->base, &gstride, e), e);
      has_stride = true;
    }

  parser->dest->type = ST_REMOVE;
  return remst_create (parser->dest, ref, gstride, e);
}

/* command ::= create_cmd | delete_cmd | insert_cmd | append_cmd | read_cmd | write_cmd | take_cmd | remove_cmd */
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
    case TT_APPEND:
      {
        return parse_append_cmd (parser, e);
      }
    case TT_READ:
      {
        return parse_read_cmd (parser, e);
      }
    case TT_WRITE:
      {
        return parse_write_cmd (parser, e);
      }
    case TT_TAKE:
      {
        return parse_take_cmd (parser, e);
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
