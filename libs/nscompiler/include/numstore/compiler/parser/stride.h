#pragma once

#include <numstore/compiler/parser/parser.h>
#include <numstore/compiler/tokens.h>
#include <numstore/core/error.h>
#include <numstore/core/stride.h>
#include <numstore/intf/types.h>

/**
 * stride       ::= '[' stride_inner ']'
 * stride_inner ::= start_part?
 * start_part   ::= integer? step_part
 * step_part    ::= (':' stop_part)?
 * stop_part   ::= integer? final_part
 * final_part   ::= (':' integer?)?
 */
struct stride_parser
{
  struct parser base;
  struct user_stride dest;
};

err_t parse_stride (struct token *src, u32 src_len, struct stride_parser *parser, error *e);
