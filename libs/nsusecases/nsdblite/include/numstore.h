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
 *   Main API header for the NumStore database library. Provides type definitions,
 *   data accessors, operation parameters, and the complete nslib API for database
 *   operations including create, delete, read, write, insert, and remove. Supports
 *   blocking, stop-and-resume, and asynchronous operation patterns.
 */

// Core
#include <numstore/core/cbuffer.h>
#include <numstore/core/error.h>
#include <numstore/core/promise.h>
#include <numstore/core/string.h>
#include <numstore/types/types.h>

////////////////////////////////////////////
//// TYPES
struct type;

////////////////////////////////////////////
//// Accessors

struct type_accessor;

struct union_accessor
{
  // The field to access
  struct string vname;

  // (Sub) Accessor for each element
  struct type_accessor *acc;
};

struct struct_accessor
{
  // The field to access
  struct string vname;

  // (Sub) Accessor for each element
  struct type_accessor *acc;
};

struct sarray_accessor
{
  // Array range
  st_size start;
  st_size end;
  st_size stride;

  // (Sub) Accessor for each element
  struct type_accessor *acc;
};

struct type_accessor
{
  union
  {
    struct struct_accessor st;
    struct union_accessor un;
    struct sarray_accessor sa;
  };
};

struct cpld_accessor
{
  struct type_accessor *acc;
  u32 acclen;
};

struct seq_accessor
{
  // Sequential accessors
  struct
  {
    struct cpld_accessor acc;
    struct cbuffer *dest;
  } * seq;
  u32 seqlen;
};
/////////////////////////////////////////////
//// Parameters

struct createp
{
  struct string vname;
  struct type t;
};

struct deletep
{
  struct string vname;
};

struct getp
{
  struct string vname;
};

struct readp
{
  struct string vname;
  struct cbuffer *dest;
  struct read_attr attr;
};

struct removep
{
  struct string vname;
  struct cbuffer *dest;
  struct remove_attr attr;
};

struct insertp
{
  struct string vname;
  struct cbuffer *src;
  struct insert_attr attr;
};

struct writep
{
  struct string vname;
  struct cbuffer *src;
  struct write_attr attr;
};

/////////////////////////////////////////////
//// Requests - Parameters and return value

struct creater
{
  // Input
  struct createp params;
};

struct deleter
{
  // Input
  struct deletep params;
};

struct getr
{
  // Input
  struct getp params;

  // Output
  b_size len;
};

struct readr
{
  // Input
  struct readp params;

  // Output
  sb_size nread;
};

struct remover
{
  // Input
  struct removep params;
};

struct insertr
{
  // Input
  struct insertp params;
};

struct writer
{
  // Input
  struct writep params;

  // Output
  sb_size nwritten;
};

struct nslib;

struct nslib_params
{
  u32 nthreads; // Number of threads to run nslib on
  struct string db_file;
  struct string wal_file;

  // Optional - existing
  //      Ignores stuff above if supplied
  struct pager *p;
  struct thread_pool *w;
};

/////////////////////////////////////////////
//// Main API

struct nslib *nslib_init (struct nslib_params params, error *e);
err_t nslib_complete (struct nslib *n, error *e);

/**
 * ================== Blocking Pattern
 *
 * Intended usage:
 *
 * nslib_create(lib, &p, (struct creater){ ... });
 *
 * promise_await(&p);
 */
err_t nslib_create (struct nslib *r, struct creater *request, error *e);
err_t nslib_delete (struct nslib *r, struct deleter *request, error *e);
err_t nslib_get (struct nslib *r, struct getr *request, error *e);
err_t nslib_read (struct nslib *r, struct readr *request, error *e);
err_t nslib_remove (struct nslib *r, struct remover *request, error *e);
err_t nslib_insert (struct nslib *r, struct insertr *request, error *e);
err_t nslib_write (struct nslib *r, struct writer *request, error *e);

/**
 * ================== Stop and Resumable Pattern
 *
 * Intended usage:
 *
 * struct nslib_stfl * state = nslib_stfl_open(lib, e);
 * nslib_stfl_create(state, (struct creater){ ... });
 *
 * while(nslib_stfl_is_idle(state)) {
 *    nslib_stfl_execute(state);
 * }
 */
struct nslib_stfl;

// Lifecycle
struct nslib_stfl *nslib_stfl_open (struct nslib *r, error *e);
err_t nslib_stfl_close (struct nslib_stfl *s, error *e);

// Utilities
bool nslib_stfl_is_idle (struct nslib_stfl *s);
err_t nslib_stfl_execute (struct nslib_stfl *s, error *e);

// Seed functions
void nslib_stfl_create (struct nslib_stfl *r, struct creater request);
void nslib_stfl_delete (struct nslib_stfl *r, struct deleter request);
void nslib_stfl_read (struct nslib_stfl *r, struct readr request);
void nslib_stfl_remove (struct nslib_stfl *r, struct remover request);
void nslib_stfl_insert (struct nslib_stfl *r, struct insertr request);
void nslib_stfl_write (struct nslib_stfl *r, struct writer request);

/**
 * ================== Asynchronous Pattern
 *
 * Intended usage:
 *
 * struct promise p;
 * promise_create(&p, &e);
 *
 * nslib_async_create(lib, &p, (struct creater){ ... });
 *
 * promise_await(&p);
 */
void nslib_async_create (struct nslib *r, struct promise *p, struct creater request);
void nslib_async_delete (struct nslib *r, struct promise *p, struct deleter request);
void nslib_async_read (struct nslib *r, struct promise *p, struct readr request);
void nslib_async_remove (struct nslib *r, struct promise *p, struct remover request);
void nslib_async_insert (struct nslib *r, struct promise *p, struct insertr request);
void nslib_async_write (struct nslib *r, struct promise *p, struct writer request);
