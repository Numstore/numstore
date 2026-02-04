#include "numstore/compiler/lexer.h"
#include "numstore/compiler/parser/parser.h"
#include "numstore/compiler/parser/statement.h"
#include "numstore/compiler/parser/stride.h"
#include "numstore/compiler/parser/type.h"
#include "numstore/core/chunk_alloc.h"
#include <numstore/compiler/compiler.h>
#include <numstore/test/testing.h>
#include <numstore/types/prim.h>

err_t
compile_statement (
    struct statement *dest,
    const char *text,
    struct chunk_alloc *alloc,
    error *e)
{
  struct lexer lex;
  err_t_wrap (lex_tokens (text, i_strlen (text), &lex, e), e);

  struct parser parser = parser_init (lex.tokens, lex.src_len);

  return parse_statement (&parser, dest, alloc, e);
}

err_t
compile_type (
    struct type *dest,
    const char *text,
    struct chunk_alloc *dalloc,
    error *e)
{
  struct lexer lex;
  err_t_wrap (lex_tokens (text, i_strlen (text), &lex, e), e);

  struct parser parser = parser_init (lex.tokens, lex.src_len);

  return parse_type (&parser, dest, dalloc, e);
}

err_t
compile_stride (
    struct user_stride *dest,
    const char *text,
    error *e)
{
  struct lexer lex;
  err_t_wrap (lex_tokens (text, i_strlen (text), &lex, e), e);

  struct parser parser = parser_init (lex.tokens, lex.src_len);

  return parse_stride (&parser, dest, e);
}

#ifndef NTEST
TEST (TT_UNIT, compile_stride_basic)
{
  error err = error_create ();
  struct user_stride stride = { 0 };

  TEST_CASE ("empty stride []")
  {
    test_assert_int_equal (compile_stride (&stride, "[]", &err), SUCCESS);
    test_assert_int_equal (stride.present, 0);
  }

  TEST_CASE ("single integer [5]")
  {
    stride = (struct user_stride){ 0 };
    test_assert_int_equal (compile_stride (&stride, "[5]", &err), SUCCESS);
    test_assert_int_equal (stride.start, 5);
    test_assert_int_equal (stride.present & START_PRESENT, START_PRESENT);
    test_assert_int_equal (stride.present & STOP_PRESENT, 0);
    test_assert_int_equal (stride.present & STEP_PRESENT, 0);
  }

  TEST_CASE ("step cannot be zero")
  {
    stride = (struct user_stride){ 0 };
    test_assert_int_equal (compile_stride (&stride, "[::0]", &err), ERR_INVALID_ARGUMENT);
    err.cause_code = SUCCESS;
  }
}

TEST (TT_UNIT, compile_type_primitives)
{
  error err = error_create ();
  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);
  struct type t;

  TEST_CASE ("i8")
  {
    test_assert_int_equal (compile_type (&t, "i8", &arena, &err), SUCCESS);
    test_assert_int_equal (t.type, T_PRIM);
    test_assert_int_equal (t.p, I8);
  }

  TEST_CASE ("i16")
  {
    test_assert_int_equal (compile_type (&t, "i16", &arena, &err), SUCCESS);
    test_assert_int_equal (t.type, T_PRIM);
    test_assert_int_equal (t.p, I16);
  }

  TEST_CASE ("i32")
  {
    test_assert_int_equal (compile_type (&t, "i32", &arena, &err), SUCCESS);
    test_assert_int_equal (t.type, T_PRIM);
    test_assert_int_equal (t.p, I32);
  }

  TEST_CASE ("i64")
  {
    test_assert_int_equal (compile_type (&t, "i64", &arena, &err), SUCCESS);
    test_assert_int_equal (t.type, T_PRIM);
    test_assert_int_equal (t.p, I64);
  }

  TEST_CASE ("u8")
  {
    test_assert_int_equal (compile_type (&t, "u8", &arena, &err), SUCCESS);
    test_assert_int_equal (t.type, T_PRIM);
    test_assert_int_equal (t.p, U8);
  }

  TEST_CASE ("u16")
  {
    test_assert_int_equal (compile_type (&t, "u16", &arena, &err), SUCCESS);
    test_assert_int_equal (t.type, T_PRIM);
    test_assert_int_equal (t.p, U16);
  }

  TEST_CASE ("u32")
  {
    test_assert_int_equal (compile_type (&t, "u32", &arena, &err), SUCCESS);
    test_assert_int_equal (t.type, T_PRIM);
    test_assert_int_equal (t.p, U32);
  }

  TEST_CASE ("u64")
  {
    test_assert_int_equal (compile_type (&t, "u64", &arena, &err), SUCCESS);
    test_assert_int_equal (t.type, T_PRIM);
    test_assert_int_equal (t.p, U64);
  }

  TEST_CASE ("f32")
  {
    test_assert_int_equal (compile_type (&t, "f32", &arena, &err), SUCCESS);
    test_assert_int_equal (t.type, T_PRIM);
    test_assert_int_equal (t.p, F32);
  }

  TEST_CASE ("f64")
  {
    test_assert_int_equal (compile_type (&t, "f64", &arena, &err), SUCCESS);
    test_assert_int_equal (t.type, T_PRIM);
    test_assert_int_equal (t.p, F64);
  }

  chunk_alloc_free_all (&arena);
}

TEST (TT_UNIT, compile_type_sarray)
{
  error err = error_create ();
  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);
  struct type t;

  TEST_CASE ("[10]i32")
  {
    test_assert_int_equal (compile_type (&t, "[10]i32", &arena, &err), SUCCESS);
    test_assert_int_equal (t.type, T_SARRAY);
    test_assert_int_equal (t.sa.rank, 1);
    test_assert_int_equal (t.sa.dims[0], 10);
    test_assert_int_equal (t.sa.t->type, T_PRIM);
    test_assert_int_equal (t.sa.t->p, I32);
  }

  TEST_CASE ("[5][10]f64")
  {
    test_assert_int_equal (compile_type (&t, "[5][10]f64", &arena, &err), SUCCESS);
    test_assert_int_equal (t.type, T_SARRAY);
    test_assert_int_equal (t.sa.rank, 2);
    test_assert_int_equal (t.sa.dims[0], 5);
    test_assert_int_equal (t.sa.dims[1], 10);
    test_assert_int_equal (t.sa.t->type, T_PRIM);
    test_assert_int_equal (t.sa.t->p, F64);
  }

  TEST_CASE ("[2][3][4]u8")
  {
    test_assert_int_equal (compile_type (&t, "[2][3][4]u8", &arena, &err), SUCCESS);
    test_assert_int_equal (t.type, T_SARRAY);
    test_assert_int_equal (t.sa.rank, 3);
    test_assert_int_equal (t.sa.dims[0], 2);
    test_assert_int_equal (t.sa.dims[1], 3);
    test_assert_int_equal (t.sa.dims[2], 4);
  }

  chunk_alloc_free_all (&arena);
}

TEST (TT_UNIT, compile_type_enum)
{
  error err = error_create ();
  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);
  struct type t;

  TEST_CASE ("enum { A B C }")
  {
    test_assert_int_equal (compile_type (&t, "enum { A B C }", &arena, &err), SUCCESS);
    test_assert_int_equal (t.type, T_ENUM);
    test_assert_int_equal (t.en.len, 3);
    test_assert (string_equal (t.en.keys[0], strfcstr ("A")));
    test_assert (string_equal (t.en.keys[1], strfcstr ("B")));
    test_assert (string_equal (t.en.keys[2], strfcstr ("C")));
  }

  chunk_alloc_free_all (&arena);
}

TEST (TT_UNIT, compile_type_struct)
{
  error err = error_create ();
  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);
  struct type t;

  TEST_CASE ("struct { x i32 y f64 }")
  {
    test_assert_int_equal (compile_type (&t, "struct { x i32 y f64 }", &arena, &err), SUCCESS);
    test_assert_int_equal (t.type, T_STRUCT);
    test_assert_int_equal (t.st.len, 2);
    test_assert (string_equal (t.st.keys[0], strfcstr ("x")));
    test_assert (string_equal (t.st.keys[1], strfcstr ("y")));
    test_assert_int_equal (t.st.types[0].type, T_PRIM);
    test_assert_int_equal (t.st.types[0].p, I32);
    test_assert_int_equal (t.st.types[1].type, T_PRIM);
    test_assert_int_equal (t.st.types[1].p, F64);
  }

  TEST_CASE ("nested struct { a struct { b i32 } }")
  {
    test_assert_int_equal (compile_type (&t, "struct { a struct { b i32 } }", &arena, &err), SUCCESS);
    test_assert_int_equal (t.type, T_STRUCT);
    test_assert_int_equal (t.st.len, 1);
    test_assert (string_equal (t.st.keys[0], strfcstr ("a")));
    test_assert_int_equal (t.st.types[0].type, T_STRUCT);
    test_assert_int_equal (t.st.types[0].st.len, 1);
  }

  chunk_alloc_free_all (&arena);
}

TEST (TT_UNIT, compile_type_union)
{
  error err = error_create ();
  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);
  struct type t;

  TEST_CASE ("union { a i32 b f64 }")
  {
    test_assert_int_equal (compile_type (&t, "union { a i32 b f64 }", &arena, &err), SUCCESS);
    test_assert_int_equal (t.type, T_UNION);
    test_assert_int_equal (t.un.len, 2);
    test_assert (string_equal (t.un.keys[0], strfcstr ("a")));
    test_assert (string_equal (t.un.keys[1], strfcstr ("b")));
    test_assert_int_equal (t.un.types[0].type, T_PRIM);
    test_assert_int_equal (t.un.types[0].p, I32);
    test_assert_int_equal (t.un.types[1].type, T_PRIM);
    test_assert_int_equal (t.un.types[1].p, F64);
  }

  chunk_alloc_free_all (&arena);
}

TEST (TT_UNIT, compile_type_complex)
{
  error err = error_create ();
  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);
  struct type t;

  TEST_CASE ("[10]struct { x i32 y [5]f64 }")
  {
    test_assert_int_equal (compile_type (&t, "[10]struct { x i32 y [5]f64 }", &arena, &err), SUCCESS);
    test_assert_int_equal (t.type, T_SARRAY);
    test_assert_int_equal (t.sa.rank, 1);
    test_assert_int_equal (t.sa.dims[0], 10);
    test_assert_int_equal (t.sa.t->type, T_STRUCT);
    test_assert_int_equal (t.sa.t->st.len, 2);
  }

  chunk_alloc_free_all (&arena);
}

TEST (TT_UNIT, compile_statement_create)
{
  error err = error_create ();
  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);
  struct statement stmt;

  TEST_CASE ("create myvar i32")
  {
    test_assert_int_equal (compile_statement (&stmt, "create myvar i32", &arena, &err), SUCCESS);
    test_assert_int_equal (stmt.type, ST_CREATE);
    test_assert (string_equal (stmt.create.vname, strfcstr ("myvar")));
    test_assert_int_equal (stmt.create.vtype.type, T_PRIM);
    test_assert_int_equal (stmt.create.vtype.p, I32);
  }

  TEST_CASE ("create arr [100]f64")
  {
    test_assert_int_equal (compile_statement (&stmt, "create arr [100]f64", &arena, &err), SUCCESS);
    test_assert_int_equal (stmt.type, ST_CREATE);
    test_assert (string_equal (stmt.create.vname, strfcstr ("arr")));
    test_assert_int_equal (stmt.create.vtype.type, T_SARRAY);
    test_assert_int_equal (stmt.create.vtype.sa.dims[0], 100);
  }

  TEST_CASE ("create complex struct { x i32 y f64 }")
  {
    test_assert_int_equal (compile_statement (&stmt, "create complex struct { x i32 y f64 }", &arena, &err), SUCCESS);
    test_assert_int_equal (stmt.type, ST_CREATE);
    test_assert (string_equal (stmt.create.vname, strfcstr ("complex")));
    test_assert_int_equal (stmt.create.vtype.type, T_STRUCT);
  }

  chunk_alloc_free_all (&arena);
}

TEST (TT_UNIT, compile_statement_delete)
{
  error err = error_create ();
  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);
  struct statement stmt;

  TEST_CASE ("delete myvar")
  {
    test_assert_int_equal (compile_statement (&stmt, "delete myvar", &arena, &err), SUCCESS);
    test_assert_int_equal (stmt.type, ST_DELETE);
    test_assert (string_equal (stmt.delete.vname, strfcstr ("myvar")));
  }

  chunk_alloc_free_all (&arena);
}

TEST (TT_UNIT, compile_statement_insert)
{
  error err = error_create ();
  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);
  struct statement stmt;

  TEST_CASE ("insert myvar 10 5")
  {
    test_assert_int_equal (compile_statement (&stmt, "insert myvar 10 5", &arena, &err), SUCCESS);
    test_assert_int_equal (stmt.type, ST_INSERT);
    test_assert (string_equal (stmt.insert.vname, strfcstr ("myvar")));
    test_assert_int_equal (stmt.insert.ofst, 10);
    test_assert_int_equal (stmt.insert.nelems, 5);
  }

  TEST_CASE ("insert arr 0 100")
  {
    test_assert_int_equal (compile_statement (&stmt, "insert arr 0 100", &arena, &err), SUCCESS);
    test_assert_int_equal (stmt.type, ST_INSERT);
    test_assert_int_equal (stmt.insert.ofst, 0);
    test_assert_int_equal (stmt.insert.nelems, 100);
  }

  chunk_alloc_free_all (&arena);
}

TEST (TT_UNIT, compile_statement_append)
{
  error err = error_create ();
  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);
  struct statement stmt;

  TEST_CASE ("append myvar 10")
  {
    test_assert_int_equal (compile_statement (&stmt, "append myvar 10", &arena, &err), SUCCESS);
    test_assert_int_equal (stmt.type, ST_APPEND);
    test_assert (string_equal (stmt.append.vname, strfcstr ("myvar")));
    test_assert_int_equal (stmt.append.nelems, 10);
  }

  chunk_alloc_free_all (&arena);
}

TEST (TT_UNIT, compile_statement_read)
{
  error err = error_create ();
  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);
  struct statement stmt;

  TEST_CASE ("read myvar")
  {
    test_assert_int_equal (compile_statement (&stmt, "read myvar", &arena, &err), SUCCESS);
    test_assert_int_equal (stmt.type, ST_READ);
    test_assert_int_equal (stmt.read.vrefs.len, 1);
    test_assert (string_equal (stmt.read.vrefs.items[0].vname, strfcstr ("myvar")));
  }

  chunk_alloc_free_all (&arena);
}

TEST (TT_UNIT, compile_statement_write)
{
  error err = error_create ();
  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);
  struct statement stmt;

  TEST_CASE ("write myvar")
  {
    test_assert_int_equal (compile_statement (&stmt, "write myvar", &arena, &err), SUCCESS);
    test_assert_int_equal (stmt.type, ST_WRITE);
    test_assert_int_equal (stmt.write.vrefs.len, 1);
    test_assert (string_equal (stmt.write.vrefs.items[0].vname, strfcstr ("myvar")));
  }

  chunk_alloc_free_all (&arena);
}

TEST (TT_UNIT, compile_statement_take)
{
  error err = error_create ();
  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);
  struct statement stmt;

  TEST_CASE ("take myvar")
  {
    test_assert_int_equal (compile_statement (&stmt, "take myvar", &arena, &err), SUCCESS);
    test_assert_int_equal (stmt.type, ST_TAKE);
    test_assert_int_equal (stmt.take.vrefs.len, 1);
    test_assert (string_equal (stmt.take.vrefs.items[0].vname, strfcstr ("myvar")));
  }

  chunk_alloc_free_all (&arena);
}

TEST (TT_UNIT, compile_statement_remove)
{
  error err = error_create ();
  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);
  struct statement stmt;

  TEST_CASE ("remove myvar")
  {
    test_assert_int_equal (compile_statement (&stmt, "remove myvar", &arena, &err), SUCCESS);
    test_assert_int_equal (stmt.type, ST_REMOVE);
  }

  chunk_alloc_free_all (&arena);
}

TEST (TT_UNIT, compile_statement_errors)
{
  error err = error_create ();
  struct chunk_alloc arena;
  chunk_alloc_create_default (&arena);
  struct statement stmt;

  TEST_CASE ("missing variable name after create")
  {
    test_assert (compile_statement (&stmt, "create", &arena, &err) != SUCCESS);
    err.cause_code = SUCCESS;
  }

  TEST_CASE ("missing type after variable name")
  {
    test_assert (compile_statement (&stmt, "create myvar", &arena, &err) != SUCCESS);
    err.cause_code = SUCCESS;
  }

  TEST_CASE ("invalid statement keyword")
  {
    test_assert (compile_statement (&stmt, "invalid myvar", &arena, &err) != SUCCESS);
    err.cause_code = SUCCESS;
  }

  chunk_alloc_free_all (&arena);
}
#endif
