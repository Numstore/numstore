#pragma once

struct predicate
{
  struct cbuffer **inputs;
  struct type **types;
  void *ctx;
};

bool predicate_evaluate (struct predicate p);
