#pragma once
#include <numstore/compiler/parser/parser.h>
#include <numstore/compiler/tokens.h>
#include <numstore/core/chunk_alloc.h>
#include <numstore/types/type_ref.h>
/**
 * EBNF Grammar:
 * =============
 *
 * type_ref        ::= struct_type_ref
 *                   | take_type_ref
 *
 * struct_type_ref ::= 'struct' '{' field_ref (',' field_ref)* '}'
 *
 * take_type_ref   ::= subtype
 *
 * field_ref       ::= IDENTIFIER type_ref
 */
err_t parse_type_ref (struct parser *p, struct type_ref *dest, struct chunk_alloc *dalloc, error *e);
