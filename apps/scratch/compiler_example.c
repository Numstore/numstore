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
 *   Scratch/example program demonstrating compiler functionality.
 */

// core
#include <numstore/core/cbuffer.h>
#include <numstore/core/error.h>

#include <compiler/compiler.h>

// compiler
int
main (void)
{
  error e = error_create ();
  struct compiler_s *c = compiler_create (&e);

  struct cbuffer *cinput = compiler_get_input (c);
  // cbuffer *coutput = compiler_get_output (c);

  char *stmnt = "create a u32;";

  struct cbuffer input = cbuffer_create_from_cstr (stmnt);

  while (cbuffer_len (&input) > 0)
    {
      cbuffer_cbuffer_move_max (cinput, &input);
      compiler_execute (c);
    }
}
