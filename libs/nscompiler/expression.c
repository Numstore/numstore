/*
 * Copyright 2025 Theo Lincke
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 *   Implements expression.h. Provides expression evaluation logic for binary,
 *   unary, and grouping operations on literals.
 */

#include <numstore/compiler/expression.h>

#include <numstore/core/assert.h>

HEADER_FUNC struct expr
create_literal_expr (struct literal l)
{
  return (struct expr){ .type = ET_LITERAL, .l = l };
}

HEADER_FUNC struct expr
create_grouping_expr (struct expr *e)
{
  return (struct expr){ .type = ET_GROUPING, .g = e };
}

HEADER_FUNC struct expr
create_unary_expr (struct expr *e, enum token_t op)
{
  struct unary ret = {
    .op = op,
    .e = e,
  };

  DBG_ASSERT (unary, &ret);

  return (struct expr){
    .type = ET_UNARY,
    .u = ret,
  };
}

HEADER_FUNC struct expr
create_binary_expr (struct expr *left, enum token_t op, struct expr *right)
{
  struct binary ret = {
    .left = left,
    .op = op,
    .right = right,
  };

  DBG_ASSERT (binary, &ret);

  return (struct expr){
    .type = ET_BINARY,
    .b = ret,
  };
}

static inline err_t
binary_evaluate (struct literal *dest, struct binary *b, struct lalloc *alloc, error *err)
{
  struct literal left, right;

  err_t_wrap (expr_evaluate (&left, b->left, alloc, err), err);
  err_t_wrap (expr_evaluate (&right, b->right, alloc, err), err);

  *dest = left;

  switch (b->op)
    {
    case TT_PLUS:
      {
        return literal_plus_literal (dest, &right, alloc, err);
      }
    case TT_MINUS:
      {
        return literal_minus_literal (dest, &right, err);
      }
    case TT_STAR:
      {
        return literal_star_literal (dest, &right, err);
      }
    case TT_SLASH:
      {
        return literal_slash_literal (dest, &right, err);
      }
    case TT_EQUAL_EQUAL:
      {
        return literal_equal_equal_literal (dest, &right, err);
      }
    case TT_BANG_EQUAL:
      {
        return literal_bang_equal_literal (dest, &right, err);
      }
    case TT_GREATER:
      {
        return literal_greater_literal (dest, &right, err);
      }
    case TT_GREATER_EQUAL:
      {
        return literal_greater_equal_literal (dest, &right, err);
      }
    case TT_LESS:
      {
        return literal_less_literal (dest, &right, err);
      }
    case TT_LESS_EQUAL:
      {
        return literal_less_equal_literal (dest, &right, err);
      }
    case TT_CARET:
      {
        return literal_caret_literal (dest, &right, err);
      }
    case TT_PERCENT:
      {
        return literal_mod_literal (dest, &right, err);
      }
    case TT_PIPE:
      {
        return literal_pipe_literal (dest, &right, err);
      }
    case TT_PIPE_PIPE:
      {
        literal_pipe_pipe_literal (dest, &right);
        return SUCCESS;
      }
    case TT_AMPERSAND:
      {
        return literal_ampersand_literal (dest, &right, err);
      }
    case TT_AMPERSAND_AMPERSAND:
      {
        literal_ampersand_ampersand_literal (dest, &right);
        return SUCCESS;
      }
    default:
      {
        UNREACHABLE ();
      }
    }
}

static inline err_t
unary_evaluate (struct literal *dest, struct unary *u, struct lalloc *alloc, error *err)
{
  struct literal operand;
  err_t_wrap (expr_evaluate (&operand, u->e, alloc, err), err);

  *dest = operand;

  switch (u->op)
    {
    case TT_NOT:
      {
        return literal_not (dest, err);
      }
    case TT_MINUS:
      {
        return literal_minus (dest, err);
      }
    case TT_BANG:
      {
        literal_bang (dest);
        return SUCCESS;
      }
    default:
      {
        UNREACHABLE ();
      }
    }
}

err_t
expr_evaluate (struct literal *dest, struct expr *ex, struct lalloc *alloc, error *err)
{
  switch (ex->type)
    {
    case ET_LITERAL:
      {
        *dest = ex->l;
        return SUCCESS;
      }
    case ET_UNARY:
      {
        return unary_evaluate (dest, &ex->u, alloc, err);
      }
    case ET_BINARY:
      {
        return binary_evaluate (dest, &ex->b, alloc, err);
      }
    case ET_GROUPING:
      {
        return expr_evaluate (dest, ex->g, alloc, err);
      }
    default:
      {
        UNREACHABLE ();
      }
    }
}
