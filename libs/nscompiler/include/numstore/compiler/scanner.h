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
 *   Scanner (lexer) structures and state management. Defines scanner state
 *   machine for tokenizing input character streams, handling identifiers,
 *   strings, numbers, and error recovery.
 */

// system
#include "statement.h"

typedef struct
{
  enum
  {
    SS_START,   /* Start here and finish here */
    SS_IDENT,   /* Parsing an identifier or magic string */
    SS_STRING,  /* Parsing a "string" */
    SS_NUMBER,  /* Parsing the integer part of an int or float */
    SS_DECIMAL, /* Parsing the right half of a dec */
    SS_ERR,     /* Rewinding to next TT_SEMICOLON */
  } state;

  /* Used in SS_IDENT, SS_STRING, SS_INTEGER, SS_DECIMAL */
  u32 slen;
  char str[512];
} scanner_state;

typedef struct
{
  scanner_state state;       /* The state for resumability */
  struct cbuffer *input;     /* Input Chars */
  struct cbuffer *output;    /* Output tokens */
  char prev_token;           /* Char cache for 2 char lookahead for small tokens */
  error e;                   /* Scanner related errors */
  struct statement *current; /* Current Statement */
} scanner;

void scanner_init (scanner *dest, struct cbuffer *input, struct cbuffer *output);
void scanner_execute (scanner *s);
