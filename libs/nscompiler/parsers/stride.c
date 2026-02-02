#include "numstore/core/stride.h"
#include <numstore/compiler/parser/stride.h>

static err_t parse_stride_inner (struct stride_parser *parser, error *e);

/* final_part ::= ':' integer | ':' | empty */
static err_t
parse_final_part (struct stride_parser *parser, error *e)
{
  /* Check for second colon */
  if (!parser_match (&parser->base, TT_COLON))
    {
      /* No colon means we're done - should see ']' */
      return SUCCESS;
    }

  /* Consume colon */
  parser_advance (&parser->base);

  /* Check for integer (this is the stop value) */
  if (parser_match (&parser->base, TT_INTEGER))
    {
      struct token *tok = parser_advance (&parser->base);
      parser->dest.stop = (sb_size)tok->integer;
      parser->dest.present |= STOP_PRESENT;
    }

  /* Done - should see ']' */
  return SUCCESS;
}

/* stop_part ::= integer final_part | final_part */
static err_t
parse_stop_part (struct stride_parser *parser, error *e)
{
  err_t err;

  /* Check for integer (this is the step value) */
  if (parser_match (&parser->base, TT_INTEGER))
    {
      struct token *tok = parser_advance (&parser->base);
      if (tok->integer == 0)
        {
          return error_causef (e, ERR_INVALID_ARGUMENT, "Step cannot be zero at position %u", parser->base.pos - 1);
        }
      parser->dest.step = (sb_size)tok->integer;
      parser->dest.present |= STEP_PRESENT;
    }

  /* Parse final_part */
  return parse_final_part (parser, e);
}

/* step_part ::= ':' stop_part | empty */
static err_t
parse_step_part (struct stride_parser *parser, error *e)
{
  err_t err;

  /* Check for colon */
  if (!parser_match (&parser->base, TT_COLON))
    {
      /* No colon means we're done - should see ']' */
      return SUCCESS;
    }

  /* Consume colon */
  parser_advance (&parser->base);

  /* Parse stop_part */
  return parse_stop_part (parser, e);
}

/* start_part ::= integer step_part | step_part */
static err_t
parse_start_part (struct stride_parser *parser, error *e)
{
  err_t err;

  /* Check for integer */
  if (parser_match (&parser->base, TT_INTEGER))
    {
      struct token *tok = parser_advance (&parser->base);
      parser->dest.start = (sb_size)tok->integer;
      parser->dest.present |= START_PRESENT;
    }

  /* Parse step_part */
  return parse_step_part (parser, e);
}

/* stride_inner ::= empty | start_part */
static err_t
parse_stride_inner (struct stride_parser *parser, error *e)
{
  /* Check for empty: [] */
  if (parser_match (&parser->base, TT_RIGHT_BRACKET))
    {
      return SUCCESS;
    }

  /* Parse start_part */
  return parse_start_part (parser, e);
}

/* Main entry point: stride ::= '[' stride_inner ']' */
err_t
parse_stride (struct token *src, u32 src_len, struct stride_parser *parser, error *e)
{
  err_t err;

  if (!src || !parser || !e)
    {
      return error_causef (e, ERR_INVALID_ARGUMENT, "Invalid arguments to parse_stride");
    }

  /* Initialize parser state */
  parser_init (&parser->base, src, src_len);
  parser->dest = (struct user_stride){ 0 };

  /* Expect opening bracket: '[' */
  err_t_wrap (parser_expect (&parser->base, TT_LEFT_BRACKET, e), e);

  /* Parse stride_inner */
  err_t_wrap (parse_stride_inner (parser, e), e);

  /* Expect closing bracket: ']' */
  err_t_wrap (parser_expect (&parser->base, TT_RIGHT_BRACKET, e), e);

  /* Check we're at end */
  if (!parser_at_end (&parser->base))
    {
      return error_causef (e, ERR_SYNTAX, "Unexpected tokens after stride at position %u", parser->base.pos);
    }

  return SUCCESS;
}
