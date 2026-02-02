#pragma once

#include <numstore/compiler/parser/parser.h>
#include <numstore/compiler/tokens.h>
#include <numstore/core/chunk_alloc.h>
#include <numstore/types/types.h>

/**
 * EBNF Grammar:
 * =============
 *
 * type            ::= struct_type
 *                   | union_type
 *                   | enum_type
 *                   | sarray_type
 *                   | primitive_type
 *
 * struct_type     ::= 'struct' '{' field_list? '}'
 *
 * union_type      ::= 'union' '{' field_list? '}'
 *
 * enum_type       ::= 'enum' '{' enum_list? '}'
 *
 * sarray_type     ::= '[' INTEGER ']'+ type
 *
 * primitive_type  ::= PRIM
 *
 * field_list      ::= field (',' field)* ','?
 *
 * field           ::= IDENTIFIER type
 *
 * enum_list       ::= IDENTIFIER (',' IDENTIFIER)* ','?
 */
struct type_parser
{
  struct parser base;
  struct type dest;
  struct chunk_alloc temp;
  struct chunk_alloc *persistent;
};

err_t parse_type (
    struct token *src,
    u32 src_len,
    struct chunk_alloc *dest,
    struct type_parser *parser,
    error *e);
