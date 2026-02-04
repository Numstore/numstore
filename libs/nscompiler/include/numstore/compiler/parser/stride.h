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

err_t parse_stride (struct parser *parser, struct user_stride *dest, error *e);
