#include "numstore/core/error.h"
#include "numstore/nsdb.h"
#include "numstore/pager.h"
#include <stdio.h>

struct __attribute__ ((packed)) variable1
{
  i32 a;
  f32 b;
};

struct variable1 source[100];

int
main (void)
{
  for (u32 i = 0; i < arrlen (source); ++i)
    {
      source[i].a = i;
      source[i].b = i + 1;
    }

  error e = error_create ();

  struct nsdb *n = nsdb_open ("test.db", "test.wal", &e);

  struct nsdb_io io = {
    .src = source,
    .scap = 100,
    .slen = 100,
  };

  struct txn *tx = nsdb_begin_txn (n, &e);
  nsdb_execute (n, NULL, "create variable1 struct { a i32, b f32 }", NULL, &e);
  error_reset (&e);
  nsdb_execute (n, NULL, "delete variable1", NULL, &e);
  nsdb_execute (n, NULL, "create variable1 struct { a i32, b f32 }", NULL, &e);
  nsdb_execute (n, NULL, "insert variable1 OFST 0 LEN 100", &io, &e);
  nsdb_commit (n, tx, &e);

  nsdb_close (n, &e);
}
