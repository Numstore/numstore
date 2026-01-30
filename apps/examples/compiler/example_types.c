

#include "numstore/core/chunk_alloc.h"
#include "numstore/core/error.h"
#include <numstore/compiler/lexer.h>
#include <numstore/compiler/parser/type.h>

int
main (void)
{
  error e = error_create ();
  struct lexer l;
  struct type_parser p;

  const char *str = "struct { b union { d i32, e [8]i32, f enum { FOO, BAR, BIZ } } }";
  lex_tokens (str, i_strlen (str), &l, &e);

  err_t ret = parse_type (l.tokens, l.ntokens, &p, &e);
  if (ret < SUCCESS)
    {
      return 1;
    }

  i_log_type (p.dest, &e);

  chunk_alloc_free_all (&p.alloc);

  return 0;
}
