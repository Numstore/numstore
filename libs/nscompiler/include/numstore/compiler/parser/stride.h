#pragma once

#include <numstore/compiler/parser/parser.h>
#include <numstore/compiler/tokens.h>
#include <numstore/core/error.h>
#include <numstore/core/stride.h>
#include <numstore/intf/types.h>

/**
 * stride       ::= '[' slice_range ']'
 *
 * slice_range  ::= NUMBER
 *                | NUMBER? ':' NUMBER?
 *                | NUMBER? ':' NUMBER? ':' NUMBER?
 */

struct stride_parser
{
  struct parser base;
  struct user_stride dest;
};

err_t parse_stride (struct token *src, u32 src_len, struct stride_parser *parser, error *e);
