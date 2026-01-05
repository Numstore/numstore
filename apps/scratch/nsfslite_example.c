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
 *   Scratch/example program demonstrating nsfslite API usage.
 */

#include <numstore/core/error.h>
#include <numstore/intf/os.h>
#include <numstore/intf/types.h>
#include <numstore/pager.h>

#include "nsfslite.h"

#include <assert.h>
#include <stdio.h>

static void
print_first_and_last_int_array (int *data, size_t len)
{
  assert (len > 20);

  for (size_t i = 0; i < 10; ++i)
    {
      fprintf (stdout, "%d, ", data[i]);
    }

  fprintf (stdout, " ... ");

  for (size_t i = len - 10; i < len; ++i)
    {
      fprintf (stdout, "%d, ", data[i]);
    }
  fprintf (stdout, "\n");
}

static void
init_int_range (int *data, size_t len)
{
  for (size_t i = 0; i < len; ++i)
    {
      data[i] = i;
    }
}

int
main (void)
{
  i_remove_quiet ("mydb.db", NULL);
  i_remove_quiet ("recovery.wal", NULL);
  nsfslite *n = nsfslite_open ("mydb.db", "recovery.wal");

  if (n == NULL)
    {
      return -1;
    }

  int64_t foo = -1;
  int64_t bar = -1;
  int64_t biz = -1;
  int64_t buz = -1;
  int64_t baz = -1;

  if ((foo = nsfslite_new (n, "foo")) < 0)
    {
      goto failed;
    }
  if ((bar = nsfslite_new (n, "bar")) < 0)
    {
      goto failed;
    }
  if ((biz = nsfslite_new (n, "biz")) < 0)
    {
      goto failed;
    }
  if ((buz = nsfslite_new (n, "buz")) < 0)
    {
      goto failed;
    }
  if ((baz = nsfslite_new (n, "baz")) < 0)
    {
      goto failed;
    }

  // (foo) do one large array insert
  {
    int _foo[20480];
    size_t len = sizeof (_foo) / sizeof (_foo[0]);

    init_int_range (_foo, len);

    fprintf (stdout, "foo written: ");
    print_first_and_last_int_array (_foo, len);

    if (nsfslite_insert (n, foo, 0, _foo, 0, sizeof (_foo[0]), len) < 0)
      {
        goto failed;
      }
    if (nsfslite_read (n, foo, _foo, sizeof (int),
                       (struct nsfslite_stride){
                           .bstart = 0,
                           .stride = 1,
                           .nelems = len,
                       })
        < 0)
      {
        goto failed;
      }

    fprintf (stdout, "foo read: ");
    print_first_and_last_int_array (_foo, len);
  }

  // (bar) Read with a random stride and offset
  {
    int _bar[20480];
    int len = sizeof (_bar) / sizeof (_bar[0]);

    init_int_range (_bar, len);

    fprintf (stdout, "foo written: ");
    print_first_and_last_int_array (_bar, len);

    if (nsfslite_insert (n, bar, 0, _bar, 0, sizeof (_bar[0]), len) < 0)
      {
        goto failed;
      }

    int offset = rand () % 100;
    int stride = 2 + (rand () % 10);
    int read_count = (len - offset) / stride;
    int _bar_read[20480];
    fprintf (stdout, "READ: offset %d stride %d elements %d\n", offset, stride, read_count);

    ssize_t read = nsfslite_read (n, bar, _bar_read, sizeof (int),
                                  (struct nsfslite_stride){
                                      .bstart = offset * sizeof (int),
                                      .stride = stride,
                                      .nelems = read_count,
                                  });
    if (read < 0)
      {
        goto failed;
      }
    fprintf (stdout, "bar read: ");
    print_first_and_last_int_array (_bar_read, read_count);
  }

  // (biz) Insert random lengths from 0 - 1000 1000 times at random offsets
  {
    fprintf (stdout, "biz: inserting 10000 random length arrays\n");

    size_t total_len = 0;
    int _biz[10000];
    int *_biz_read = malloc (10000 * 10000 * sizeof *_biz_read);

    error e = error_create ();
    i_timer timer;
    i_timer_create (&timer, &e);

    nsfslite_txn *tx = nsfslite_begin_txn (n);
    if (tx == NULL)
      {
        goto failed;
      }

    for (int i = 0; i < 10000; ++i)
      {
        int len = 1 + rand () % 10000;
        int offset;
        if (total_len == 0)
          {
            offset = 0;
          }
        else
          {
            offset = rand () % total_len;
          }
        total_len += len;

        init_int_range (_biz, len);

        if (len > 0 && nsfslite_insert (n, biz, tx, _biz, offset, sizeof (int), len) < 0)
          {
            goto failed;
          }
      }
    if (nsfslite_read (n, biz, _biz_read, sizeof (int),
                       (struct nsfslite_stride){
                           .bstart = 0,
                           .stride = 1,
                           .nelems = total_len,
                       })
        < 0)
      {
        goto failed;
      }

    if (nsfslite_commit (n, tx))
      {
        goto failed;
      }

    fprintf (stdout, "Time taken: %f s\n", i_timer_now_s (&timer));

    fprintf (stdout, "biz read: ");
    print_first_and_last_int_array (_biz_read, total_len);
  }

  // (buz) One big insert, one big remove the read the remaining
  {
    error e = error_create ();
    i_timer timer;
    i_timer_create (&timer, &e);

    int _buz[5000];
    size_t len = sizeof (_buz) / sizeof (_buz[0]);

    init_int_range (_buz, len);

    fprintf (stdout, "buz written: ");
    print_first_and_last_int_array (_buz, len);

    if (nsfslite_insert (n, buz, 0, _buz, 0, sizeof (_buz[0]), len) < 0)
      {
        goto failed;
      }

    int remove_offset = 1000;
    int remove_count = 2000;
    fprintf (stdout, "REMOVE: offset %d count %d\n", remove_offset, remove_count);

    if (nsfslite_remove (n, buz, 0, NULL, sizeof (int),
                         (struct nsfslite_stride){
                             .bstart = remove_offset,
                             .stride = 1,
                             .nelems = remove_count,
                         })
        < 0)
      {
        goto failed;
      }

    int remaining_len = len - remove_count;
    int _buz_read[remaining_len];
    if (nsfslite_read (n, buz, _buz_read, sizeof (int),
                       (struct nsfslite_stride){
                           .bstart = 0,
                           .nelems = remaining_len,
                           .stride = 1,
                       })
        < 0)
      {
        goto failed;
      }

    fprintf (stdout, "Time taken: %f s\n", i_timer_now_s (&timer));

    fprintf (stdout, "buz read: ");
    print_first_and_last_int_array (_buz_read, remaining_len);
  }

  // (baz) One big insert, write data at a random offset, stride then read and
  // print
  if (0)
    {
      int _baz[10000];
      size_t len = sizeof (_baz) / sizeof (_baz[0]);

      init_int_range (_baz, len);

      fprintf (stdout, "baz written: ");
      print_first_and_last_int_array (_baz, len);

      if (nsfslite_insert (n, baz, 0, _baz, 0, sizeof (_baz[0]), len) < 0)
        {
          goto failed;
        }

      int write_offset = rand () % 1000;
      int write_stride = 5 + (rand () % 10);
      int write_count = 100;
      fprintf (stdout, "WRITE: offset %d stride %d count %d\n", write_offset, write_stride, write_count);

      int _baz_write[write_count];
      for (int i = 0; i < write_count; ++i)
        {
          _baz_write[i] = 99999;
        }

      if (nsfslite_write (n, baz, 0, _baz_write, sizeof (int),
                          (struct nsfslite_stride){
                              .bstart = write_offset,
                              .nelems = write_count,
                              .stride = write_stride,
                          })
          < 0)
        {
          goto failed;
        }

      if (nsfslite_read (n, baz, _baz, sizeof (int),
                         (struct nsfslite_stride){
                             .nelems = len,
                             .bstart = 0,
                             .stride = 1,
                         })
          < 0)
        {
          goto failed;
        }

      fprintf (stdout, "baz read: ");
      print_first_and_last_int_array (_baz, len);
    }

  nsfslite_close (n);
  return 0;

failed:
  fprintf (stderr, "%s\n", nsfslite_error (n));
  if (n)
    {
      nsfslite_close (n);
    }
  return -1;
}
