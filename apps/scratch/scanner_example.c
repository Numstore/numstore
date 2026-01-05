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
 *   Scratch/example program demonstrating scanner/lexer functionality.
 */

// core
#include <numstore/core/cbuffer.h>
#include <numstore/intf/logging.h>

#include <compiler/scanner.h>
#include <compiler/tokens.h>

// compiler
int
main (void)
{
  char *stmt = "create a u32; create foo ; delete 4 + 1 bar ";
  struct token tokens[20];

  struct cbuffer input = cbuffer_create_from_cstr (stmt);
  struct cbuffer output = cbuffer_create_from (tokens);

  scanner s;
  scanner_init (&s, &input, &output);

  while (cbuffer_len (&input) > 0 || cbuffer_len (&output) > 0)
    {
      scanner_execute (&s);
      struct token next;
      if (cbuffer_read (&next, sizeof (struct token), 1, &output))
        {
          i_log_info ("%s\n", tt_tostr (next.type));
        }
    }

  return 0;
}
