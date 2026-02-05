#include <bits/time.h>
#include <fcntl.h>
#include <nsfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

struct file_utilities
{
  size_t file_size;
  char *buf;
  size_t buf_size;
};

static inline void
file_utilities_init (int fd, struct file_utilities *utils, void *data, size_t offset, size_t data_len)
{
  struct stat st;
  check (fstat (fd, &st) == 0);

  utils->file_size = st.st_size;
  utils->buf_size = data_len + st.st_size - offset;
  utils->buf = malloc (utils->buf_size);
  check (utils->buf != NULL);

  memcpy (utils->buf, data, data_len);
}

static inline void
fd_insert (int fd, size_t n, __off_t offset, struct file_utilities *utils)
{
  size_t file_size = utils->file_size;
  size_t tail_size = file_size - offset;
  size_t total_size = tail_size + n;

  check (lseek (fd, offset, SEEK_SET) == offset);
  check (read (fd, utils->buf + n, tail_size) == (ssize_t)tail_size);

  check (lseek (fd, offset, SEEK_SET) == offset);
  check (write (fd, utils->buf, total_size) == (ssize_t)total_size);
}

int
main (void)
{
  int blen = 100000000;
  int ilen = 100000000;
  int ofst = 10;
  void *bdata = malloc (blen);
  void *idata = malloc (ilen);
  struct timespec start, end;

  const char *fname = "test.db";
  remove (fname);

  // Numstore
  if (1)
    {
      const char *walfile = NULL;
      if (walfile)
        {
          remove (walfile);
        }

      nsfile *n = nsfile_open (fname, walfile);
      check (n != NULL);

      check (nsfile_insert (n, NULL, bdata, blen, 0) == 0);

      // Insert into the middle
      clock_gettime (CLOCK_MONOTONIC, &start);
      check (nsfile_insert (n, NULL, idata, ilen, ofst) == 0);
      clock_gettime (CLOCK_MONOTONIC, &end);

      nsfile_close (n);

      long long start_ns = start.tv_sec * 1000000000LL + start.tv_nsec;
      long long end_ns = end.tv_sec * 1000000000LL + end.tv_nsec;
      long long elapsed_ns = end_ns - start_ns;
      double elapsed_ms = elapsed_ns / 1000000.0;

      fprintf (stdout, "%f\n", elapsed_ms);
    }

  // File
  if (0)
    {
      remove (fname);
      int fd = open (fname, O_CREAT | O_RDWR);

      // Write the base
      size_t written = write (fd, bdata, blen);
      check (written == (size_t)blen);

      struct file_utilities fld;
      file_utilities_init (fd, &fld, idata, ofst, ilen);

      clock_gettime (CLOCK_MONOTONIC, &start);
      fd_insert (fd, ilen, ofst, &fld);
      clock_gettime (CLOCK_MONOTONIC, &end);

      long long start_ns = start.tv_sec * 1000000000LL + start.tv_nsec;
      long long end_ns = end.tv_sec * 1000000000LL + end.tv_nsec;
      long long elapsed_ns = end_ns - start_ns;
      double elapsed_ms = elapsed_ns / 1000000.0;

      fprintf (stdout, "%f\n", elapsed_ms);
    }

  return 0;
}
