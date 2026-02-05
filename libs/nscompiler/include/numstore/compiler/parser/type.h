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
 * struct_type     ::= 'struct' '{' field (',' field)* '}'
 *
 * union_type      ::= 'union' '{' field (',' field)* '}'
 *
 * enum_type       ::= 'enum' '{' IDENT (',') IDENT '}'
 *
 * sarray_type     ::= '[' INTEGER ']'+ type
 *
 * primitive_type  ::= PRIM
 *
 * field           ::= IDENTIFIER type
 */

err_t parse_type (struct parser *p, struct type *dest, struct chunk_alloc *dalloc, error *e);
