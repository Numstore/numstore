#pragma once

#include <numstore/compiler/parser/parser.h>
#include <numstore/compiler/tokens.h>
#include <numstore/core/chunk_alloc.h>
#include <numstore/types/statement.h>

/**
 * EBNF Grammar for nsfile Commands:
 * ==================================
 *
 * command         ::= create_cmd
 *                   | delete_cmd
 *                   | insert_cmd
 *                   | append_cmd
 *                   | read_cmd
 *                   | write_cmd
 *                   | take_cmd
 *                   | remove_cmd
 *
 * create_cmd      ::= 'create' VNAME type
 *
 * delete_cmd      ::= 'delete' VNAME
 *
 * insert_cmd      ::= 'insert' VNAME NUMBER NUMBER
 *
 * append_cmd      ::= 'append' VNAME NUMBER
 *
 * read_cmd        ::= 'read' var_selection
 *
 * write_cmd       ::= 'write' var_selection
 *
 * take_cmd        ::= 'take' var_selection
 *
 * remove_cmd      ::= 'remove' VNAME slice?
 *
 * var_selection   ::= var_ref (',' var_ref)* field_selection? slice?
 *
 * var_ref         ::= IDENT ('as' IDENT)?
 *
 * field_selection ::= '[' type_accessor (',' type_accessor)* ']'
 */

err_t parse_statement (struct parser *p, struct statement *dest, struct chunk_alloc *dalloc, error *e);
