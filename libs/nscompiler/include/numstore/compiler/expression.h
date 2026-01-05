#pragma once

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
 *   Expression data structures and evaluation functions. Defines unary, binary,
 *   and grouping expression types along with constructors and evaluation logic
 *   for arithmetic, logical, and bitwise operations.
 */

#include <numstore/compiler/literal.h>
#include <numstore/compiler/tokens.h>
#include <numstore/core/assert.h>

struct expr;

/////////////////////////////////////
//////////// Unary

/* !EXPR */
/* -EXPR */
struct unary
{
  enum token_t op;
  struct expr *e;
};

DEFINE_DBG_ASSERT (
    struct unary, unary, u,
    {
      ASSERT (u);
      ASSERT (u->op == TT_MINUS || u->op == TT_BANG);
    })

/////////////////////////////////////
//////////// Binary

/**
 * EXPR + EXPR
 * EXPR == EXPR
 * etc.
 */
struct binary
{
  struct expr *left;
  enum token_t op;
  struct expr *right;
};

DEFINE_DBG_ASSERT (
    struct binary, binary, b,
    {
      switch (b->op)
        {
        case TT_EQUAL_EQUAL:
        case TT_BANG_EQUAL:
        case TT_LESS:
        case TT_LESS_EQUAL:
        case TT_GREATER:
        case TT_GREATER_EQUAL:
        case TT_PLUS:
        case TT_MINUS:
        case TT_STAR:
        case TT_SLASH:
          break;
        default:
          ASSERT (false);
        }
    })

/////////////////////////////////////
//////////// Expression

struct expr
{
  enum expr_t
  {
    ET_VALUE,
    ET_UNARY,
    ET_BINARY,
    ET_GROUPING,
  } type;

  union
  {
    struct literal l;
    struct unary u;
    struct binary b;
    struct expr *g;
    char *v;
  };
};

HEADER_FUNC struct expr
create_literal_expr (struct literal l)
{
  return (struct expr){ .type = ET_VALUE, .l = l };
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

err_t expr_evaluate (struct literal *dest, struct expr *exp, struct lalloc *work, error *e);
