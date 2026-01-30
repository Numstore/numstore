#pragma once

#include "numstore/core/chunk_alloc.h"
#include <numstore/compiler/tokens.h>
#include <numstore/types/types.h>

/**
* EBNF Grammar:
* =============
* 
* declaration     ::= VNAME type
* 
* type            ::= struct_type
*                   | union_type  
*                   | enum_type
*                   | array_type
*                   | primitive_type
* 
* struct_type     ::= 'struct' '{' field_list '}'
* 
* union_type      ::= 'union' '{' field_list '}'
* 
* enum_type       ::= 'enum' '{' enum_list '}'
* 
* array_type      ::= '[' NUMBER ']' type
* 
* primitive_type  ::= IDENTIFIER
* 
* field_list      ::= field (',' field)* ','?
* 
* field           ::= VNAME type
* 
* enum_list       ::= IDENTIFIER (',' IDENTIFIER)* ','?
*/

struct type_parser
{
  struct token *src;
  u32 src_len;
  u32 pos;

  struct type dest;

  struct chunk_alloc temp;
  struct chunk_alloc alloc;
};

err_t parse_type (struct token *src, u32 src_len, struct type_parser *parser, error *e);
