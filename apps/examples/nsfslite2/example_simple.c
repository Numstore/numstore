

#include "nsfslite.h"
#include "numstore/core/error.h"
#include "numstore/intf/os/file_system.h"
#include <stdio.h>

// Packed - no padding
struct __attribute__ ((packed)) variable1
{
  i32 a;
  f32 b;
  f32 c;
};

/* Example 1: Simple elapsed time timer */

typedef struct
{
  struct timespec start;
  struct timespec end;
} timer;

static void
timer_start (timer *t)
{
  clock_gettime (CLOCK_MONOTONIC, &t->start);
}

static void
timer_stop (timer *t)
{
  clock_gettime (CLOCK_MONOTONIC, &t->end);
}

static double
timer_elapsed_us (timer *t)
{
  double start_us = t->start.tv_sec * 1000000.0 + t->start.tv_nsec / 1000.0;
  double end_us = t->end.tv_sec * 1000000.0 + t->end.tv_nsec / 1000.0;
  return end_us - start_us;
}

int
main (void)
{
  error e = error_create ();
  if (i_remove_quiet ("test.db", &e))
    {
      return e.cause_code;
    }
  if (i_remove_quiet ("test.wal", &e))
    {
      return e.cause_code;
    }

  nsfslite *n = nsfslite_open ("test.db", NULL, &e);
  if (n == NULL)
    {
      return e.cause_code;
    }

  spgno root = nsfslite_new (n, NULL, "variable1", "struct { a i32, b f32, c f32 }", &e);
  if (root < 0)
    {
      return root;
    }

  struct variable1 input[10];
  for (u32 i = 0; i < arrlen (input); ++i)
    {
      input[i].a = i;
      input[i].b = i + 1;
      input[i].c = i + 2;
    }

  timer t;
  timer_start (&t);

  struct txn *tx = nsfslite_begin_txn (n, &e);
  if (tx == NULL)
    {
      return e.cause_code;
    }

  for (u32 i = 0; i < 1000; ++i)
    {
      if (nsfslite_insert (n, root, tx, input, 0, arrlen (input), &e))
        {
          return e.cause_code;
        }

      if (nsfslite_insert (n, root, tx, input, 4, arrlen (input), &e))
        {
          return e.cause_code;
        }

      if (nsfslite_insert (n, root, tx, input, 10, arrlen (input), &e))
        {
          return e.cause_code;
        }
    }

  if (nsfslite_commit (n, tx, &e))
    {
      return e.cause_code;
    }

  timer_stop (&t);
  double elapsed_us = timer_elapsed_us (&t);

  printf ("%f MB/s\n", 8 * 1000 * sizeof (input) / elapsed_us);

  return 0;
}
