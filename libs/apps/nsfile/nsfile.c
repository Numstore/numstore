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
 *   Implements nsfile.h. Core implementation of NumStore File
 */

#include <nsfile.h>
#include <nslite.h>

#include <numstore/core/clock_allocator.h>
#include <numstore/core/error.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/pager.h>
#include <numstore/pager/lock_table.h>
#include <numstore/rptree/oneoff.h>
#include <numstore/rptree/rptree_cursor.h>

#include <pthread.h>

struct nsfile_s
{
  nslite *n;
  pgno root;
};

DEFINE_DBG_ASSERT (
    struct nsfile_s, nsfile, n, {
      ASSERT (n);
    })

nsfile *
nsfile_open (const char *fname, const char *recovery, error *e)
{
  // Allocate memory
  nsfile *ret = i_malloc (1, sizeof *ret, e);
  if (ret == NULL)
    {
      goto failed;
    }

  ret->n = nslite_open (fname, recovery, e);
  if (ret->n == NULL)
    {
      i_free (ret);
      goto failed;
    }

  spgno root = nslite_new (ret->n, NULL, e);
  if (root < 0)
    {
      nslite_close (ret->n, e);
      i_free (ret);
      goto failed;
    }

  ret->root = root;

  return ret;

failed:
  return NULL;
}

err_t
nsfile_close (nsfile *n, error *e)
{
  DBG_ASSERT (nsfile, n);

  nslite_close (n->n, e);
  i_free (n);

  return e->cause_code;
}

sb_size
nsfile_size (nsfile *n, error *e)
{
  return nslite_size (n->n, n->root, e);
}

struct txn *
nsfile_begin_txn (nsfile *n, error *e)
{
  return nslite_begin_txn (n->n, e);
}

err_t
nsfile_commit (nsfile *n, struct txn *tx, error *e)
{
  return nslite_commit (n->n, tx, e);
}

err_t
nsfile_rollback (nsfile *n, struct txn *tx, error *e)
{
  return nslite_rollback (n->n, tx, e);
}

err_t
nsfile_insert (
    nsfile *n,
    struct txn *tx,
    const void *src,
    b_size bofst,
    t_size size,
    b_size nelem,
    error *e)
{
  return nslite_insert (n->n, n->root, tx, src, bofst, size, nelem, e);
}

err_t
nsfile_write (
    nsfile *n,
    struct txn *tx,
    const void *src,
    t_size size,
    struct stride stride,
    error *e)
{
  return nslite_write (n->n, n->root, tx, src, size, stride, e);
}

sb_size
nsfile_read (
    nsfile *n,
    void *dest,
    t_size size,
    struct stride stride,
    error *e)
{
  return nslite_read (n->n, n->root, dest, size, stride, e);
}

err_t
nsfile_remove (
    nsfile *n,
    struct txn *tx,
    void *dest,
    t_size size,
    struct stride stride,
    error *e)
{
  return nslite_remove (n->n, n->root, tx, dest, size, stride, e);
}
