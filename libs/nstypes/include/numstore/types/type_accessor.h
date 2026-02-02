#pragma once

#include <numstore/core/cbuffer.h>
#include <numstore/core/error.h>
#include <numstore/types/types.h>

////////////////////////////////////
/// Model

enum ta_type
{
  TA_TAKE,
  TA_SELECT,
  TA_RANGE
};

struct type_accessor
{
  enum ta_type type;

  union
  {
    struct select_ta
    {
      struct string key;
      struct type_accessor *sub_ta;
    } select;

    struct range_ta
    {
      t_size start;
      t_size step;
      t_size stop;
      struct type_accessor *sub_ta;
    } range;
  };
};

////////////////////////////////////
/// Builder

struct type_accessor_builder
{
  struct type_accessor *head;
  struct type_accessor *tail;
  struct chunk_alloc *temp;
  struct chunk_alloc *persistent;
};

void tab_create (
    struct type_accessor_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent);

err_t tab_accept_select (
    struct type_accessor_builder *builder,
    struct string key,
    error *e);

err_t tab_accept_range (
    struct type_accessor_builder *builder,
    t_size start,
    t_size stop,
    t_size step,
    error *e);

err_t tab_build (
    struct type_accessor **dest,
    struct type_accessor_builder *builder,
    error *e);
