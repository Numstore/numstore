

#include "nsfslite.h"
#include "numstore/core/error.h"
#include "numstore/intf/os/file_system.h"
#include "numstore/rs/rptrs.h"
#include "numstore/rs/trfmrs.h"

struct __attribute__ ((packed)) a
{
  i32 a;
  f32 b[20];
  struct
  {
    f32 i;
    f32 b;
  } c;
};

static void
vinit (struct a *dest, u32 size)
{
  for (u32 i = 0; i < size; ++i)
    {
      dest[i].a = i;
      for (u32 k = 0; k < 20; ++k)
        {
          dest[i].b[k] = i + k;
        }
      dest[i].c.i = i + 1;
      dest[i].c.b = i + 1;
    }
}

int
main (void)
{
  error e = error_create ();
  e.abort_on_failure = true;

  i_remove_quiet ("test.db", &e);
  i_remove_quiet ("test.wal", &e);

  struct a data[20000];
  vinit (data, 20000);

  nsfslite *n = nsfslite_open ("test.db", "test.wal", &e);

  // BEGIN TXN
  struct txn *tx = nsfslite_begin_txn (n, &e);

  // CREATE
  nsfslite_new (n, tx, "a", "struct { a i32, b [20]f32, c struct{ i f32, b f32 } }", &e);
  nsfslite_insert (n, "a", tx, data, 0, 20000, &e);

  // COMMIT
  nsfslite_commit (n, tx, &e);

  // READ
  {
    struct a dest[20000];
    sb_size size = nsfslite_read (n, "a", dest, "[0:]", &e);
    printf ("%ld\n", size);
  }

  {
    struct a dest[20000];
    sb_size size = nsfslite_read (n, "struct { a a.b[0], b a.b[2], c a.c.i }", dest, "[0:]", &e);
    printf ("%ld\n", size);
  }

  nsfslite_close (n, &e);

  return 0;
}
