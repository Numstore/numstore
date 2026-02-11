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
 *                   | read_cmd
 *                   | remove_cmd
 *
 * create_cmd      ::= 'create' VNAME type
 *
 * delete_cmd      ::= 'delete' VNAME
 *
 * insert_cmd      ::= 'insert' VNAME ('OFST' NUMBER)? ('LEN' NUMBER)?
 *
 * read_cmd        ::= 'read' type_ref
 *
 * remove_cmd      ::= 'remove' VNAME slice
 */

err_t parse_statement (struct parser *p, struct statement *dest, struct chunk_alloc *dalloc, error *e);
