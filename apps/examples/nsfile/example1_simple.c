#include <nsfile.h>
#include <stdio.h>

int
main (void)
{
  error e = error_create ();
  nsfile *n = nsfile_open ("test.db", "test.wal", &e);

  int a[10];
  for (size_t i = 0; i < arrlen (a); ++i)
    {
      a[i] = i;
    }

  nsfile_insert (n, NULL, a, 0, sizeof (int), arrlen (a), &e);
  nsfile_insert (n, NULL, a, 5 * sizeof (int), sizeof (int), arrlen (a), &e);
  nsfile_insert (n, NULL, a, 10 * sizeof (int), sizeof (int), arrlen (a), &e);

  // [0, ... 100, [ 0, 100, [ 0, ... 2048 ] 101, ... 2048], 101 ... 2048 ]

  sb_size size = nsfile_size (n, &e);
  int b[size];
  sb_size r = nsfile_read (n, b, sizeof (int), (struct stride){ .bstart = 0, .stride = 1, .nelems = size / sizeof (int) }, &e);

  for (ssize_t i = 0; i < r; ++i)
    {
      printf ("%d\n", b[i]);
    }

  return 0;
}
