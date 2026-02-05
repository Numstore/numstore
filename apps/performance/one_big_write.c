#include <bits/time.h>
#include <fcntl.h>
#include <nsfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void
check (int cond)
{
  if (!cond)
    {
      exit (-1);
    }
}

int
main (int args, const char **argv)
{
  if (args != 4 && args != 5)
    {
      goto usage;
    }

  int len = atoi (argv[2]);
  void *data = malloc (len);
  struct timespec start, end;

  if (strcmp (argv[1], "numstore") == 0)
    {
      const char *dbfile = argv[3];
      const char *walfile = NULL;
      remove (dbfile);
      if (args == 5)
        {
          walfile = argv[4];
          remove (walfile);
        }

      nsfile *n = nsfile_open (dbfile, walfile);
      check (n != NULL);

      clock_gettime (CLOCK_MONOTONIC, &start);
      check (nsfile_insert (n, NULL, data, len, 0) == 0);
      clock_gettime (CLOCK_MONOTONIC, &end);

      nsfile_close (n);
    }
  else if (strcmp (argv[1], "file") == 0)
    {
      const char *fname = argv[3];
      remove (fname);
      int fd = open (fname, O_CREAT | O_RDWR);

      clock_gettime (CLOCK_MONOTONIC, &start);
      size_t written = write (fd, data, len);
      clock_gettime (CLOCK_MONOTONIC, &end);

      check (written == (size_t)len);
    }
  else
    {
      goto usage;
    }

  long long start_ns = start.tv_sec * 1000000000LL + start.tv_nsec;
  long long end_ns = end.tv_sec * 1000000000LL + end.tv_nsec;
  long long elapsed_ns = end_ns - start_ns;
  double elapsed_ms = elapsed_ns / 1000000.0;

  fprintf (stdout, "%f\n", elapsed_ms);
  return 0;

usage:
  fprintf (stdout, "Numstore Usage: %s numstore length db_file [wal_file]\n", argv[0]);
  fprintf (stdout, "File Usage: %s file length db_file\n", argv[0]);
  return -1;
}
