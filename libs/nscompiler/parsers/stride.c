#include "numstore/core/stride.h"
#include <numstore/compiler/parser/stride.h>

static err_t parse_stride_inner (struct stride_parser *parser, error *e);

static err_t
parse_step (struct stride_parser *parser, error *e)
{
  if (!parser_match (&parser->base, TT_COLON))
    {
      return SUCCESS;
    }

  parser_advance (&parser->base);

  if (parser_match (&parser->base, TT_INTEGER))
    {
      struct token *tok = parser_advance (&parser->base);
      if (tok->integer == 0)
        {
          return error_causef (
              e, ERR_INVALID_ARGUMENT,
              "Step cannot be zero at position %u", parser->base.pos - 1);
        }
      parser->dest.step = (sb_size)tok->integer;
      parser->dest.present |= STEP_PRESENT;
    }

  return SUCCESS;
}

static err_t
parse_stop (struct stride_parser *parser, error *e)
{
  err_t err;

  if (parser_match (&parser->base, TT_INTEGER))
    {
      struct token *tok = parser_advance (&parser->base);
      parser->dest.stop = (sb_size)tok->integer;
      parser->dest.present |= STOP_PRESENT;
    }

  return parse_step (parser, e);
}

/* stride_inner ::= empty | NUMBER | NUMBER? ':' NUMBER? | NUMBER? ':' NUMBER? ':' NUMBER? */
static err_t
parse_stride_inner (struct stride_parser *parser, error *e)
{
  err_t err;

  /* Check for empty: [] */
  if (parser_match (&parser->base, TT_RIGHT_BRACKET))
    {
      return SUCCESS;
    }

  /* Check for optional start integer */
  if (parser_match (&parser->base, TT_INTEGER))
    {
      struct token *tok = parser_advance (&parser->base);
      parser->dest.start = (sb_size)tok->integer;
      parser->dest.present |= START_PRESENT;

      if (!parser_match (&parser->base, TT_COLON))
        {
          return SUCCESS;
        }
    }

  if (parser_match (&parser->base, TT_COLON))
    {
      parser_advance (&parser->base);

      return parse_stop (parser, e);
    }

  return error_causef (e, ERR_SYNTAX, "Expected ':' or ']' at position %u", parser->base.pos);
}

/* stride ::= '[' stride_inner ']' */
err_t
parse_stride (struct token *src, u32 src_len, struct stride_parser *parser, error *e)
{
  err_t err;

  if (!src || !parser || !e)
    {
      return error_causef (e, ERR_INVALID_ARGUMENT, "Invalid arguments to parse_stride");
    }

  parser_init (&parser->base, src, src_len);
  parser->dest = (struct user_stride){ 0 };

  err_t_wrap (parser_expect (&parser->base, TT_LEFT_BRACKET, e), e);

  err_t_wrap (parse_stride_inner (parser, e), e);

  err_t_wrap (parser_expect (&parser->base, TT_RIGHT_BRACKET, e), e);

  if (!parser_at_end (&parser->base))
    {
      return error_causef (e, ERR_SYNTAX, "Unexpected tokens after stride at position %u", parser->base.pos);
    }

  return SUCCESS;
}
