#pragma once

#include <numstore/compiler/tokens.h>
#include <numstore/core/error.h>
#include <numstore/intf/types.h>

struct lexer
{
  const char *src;
  u32 src_len;
  u32 start;
  u32 current;

  struct token tokens[256];
  u32 ntokens;
};

/* Lexer API */
err_t lex_tokens (const char *src, u32 src_len, struct lexer *lex, error *e);
