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
 *   Main compiler interface for the numstore compiler system. Provides functions
 *   to create and manage compiler instances, access input/output buffers, and
 *   execute compilation of character input into statement results.
 */

// compiler
#include <numstore/compiler/statement.h>

struct statement_result
{
  union
  {
    struct statement *stmt;
    error e;
  };
  bool ok;
};

struct compiler_s *compiler_create (error *e);
void compiler_free (struct compiler_s *c);

struct cbuffer *compiler_get_input (struct compiler_s *c);
struct cbuffer *compiler_get_output (struct compiler_s *c);

void compiler_execute (struct compiler_s *s);
