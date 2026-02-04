#include "numstore/compiler/lexer.h"
#include "numstore/compiler/parser/parser.h"
#include "numstore/compiler/parser/statement.h"
#include "numstore/core/chunk_alloc.h"
#include <numstore/compiler/compiler.h>

err_t
compile_statement (
    struct statement *dest,
    const char *text,
    struct chunk_alloc *alloc,
    error *e)
{
  struct lexer lex;
  err_t_wrap (lex_tokens (text, i_strlen (text), &lex, e), e);

  struct parser parser = parser_init (lex.tokens, lex.src_len);

  return parse_statement (&parser, dest, alloc, e);
}
