#pragma once

#include <numstore/compiler/tokens.h>
#include <numstore/core/assert.h>
#include <numstore/intf/types.h>

struct parser
{
  struct token *src;
  u32 src_len;
  u32 pos;
};

DEFINE_DBG_ASSERT (struct parser, parser, p, {
  ASSERT (p->src);
  ASSERT (p->src_len > 0);
  ASSERT (p->pos <= p->src_len);
})

HEADER_FUNC void
parser_init (struct parser *p, struct token *src, u32 src_len)
{
  p->src = src;
  p->src_len = src_len;
  p->pos = 0;

  DBG_ASSERT (parser, p);
}

HEADER_FUNC struct token *
parser_peek (struct parser *p)
{
  DBG_ASSERT (parser, p);
  ASSERT (p->pos < p->src_len);

  return (p->pos < p->src_len) ? &p->src[p->pos] : NULL;
}

HEADER_FUNC struct token *
parser_peek_n (struct parser *p, u32 n)
{
  DBG_ASSERT (parser, p);
  ASSERT (p->pos + n < p->src_len);

  u32 target_pos = p->pos + n;
  return (target_pos < p->src_len) ? &p->src[target_pos] : NULL;
}

HEADER_FUNC bool
parser_match (struct parser *p, enum token_t type)
{
  DBG_ASSERT (parser, p);

  struct token *tok = parser_peek (p);

  return tok->type == type;
}

HEADER_FUNC struct token *
parser_advance (struct parser *p)
{
  DBG_ASSERT (parser, p);

  struct token *tok = &p->src[p->pos];
  p->pos++;

  return tok;
}

/* Expect a specific token type, consume it, and advance */
HEADER_FUNC err_t
parser_expect (struct parser *p, enum token_t type, error *e)
{
  struct token *tok = parser_peek (p);

  if (tok->type != type)
    {
      return error_causef (e, ERR_SYNTAX,
                           "Expected token type %s at position %u, got %s",
                           tt_tostr (type), p->pos,
                           tt_tostr (tok->type));
    }

  p->pos++;
  return SUCCESS;
}

HEADER_FUNC bool
parser_at_end (struct parser *p)
{
  DBG_ASSERT (parser, p);
  return p->pos == p->src_len;
}

HEADER_FUNC err_t
parser_check_end (struct parser *p, error *e)
{
  if (parser_at_end (p))
    {
      return error_causef (e, ERR_SYNTAX, "Unexpected tokens after expression at position %u", p->pos);
    }

  return SUCCESS;
}
