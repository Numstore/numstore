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
 *   Implements file_pager.h. Provides low-level file I/O operations including page reading, writing, and file management.
 */

#include <file_pager.h>

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/test/testing.h>

#include <config.h>

DEFINE_DBG_ASSERT (
    struct file_pager, file_pager, p,
    {
      ASSERT (p);
    })

static inline err_t
fpgr_set_len (struct file_pager *p, error *e)
{
  DBG_ASSERT (file_pager, p);

  i64 size = i_file_size (&p->f, e);
  if (size < 0)
    {
      return e->cause_code;
    }

  if (size % PAGE_SIZE != 0)
    {
      return error_causef (
          e, ERR_CORRUPT,
          "Database file size should be a multiple of "
          "PAGE_SIZE: %" PRp_size " actual file size was: %" PRId64,
          PAGE_SIZE, size);
    }

  p->npages = size / PAGE_SIZE;
  return SUCCESS;
}

err_t
fpgr_open (struct file_pager *dest, const char *fname, error *e)
{
  if (i_open_rw (&dest->f, fname, e))
    {
      return e->cause_code;
    }
  if (fpgr_set_len (dest, e))
    {
      i_close (&dest->f, e);
      return e->cause_code;
    }

  DBG_ASSERT (file_pager, dest);

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, fpgr_open)
{
  error e = error_create ();
  _Static_assert(PAGE_SIZE > 2, "PAGE_SIZE should be > 2 for file_pager test");

  i_file fp;
  test_err_t_wrap (i_open_rw (&fp, "test.db", &e), &e);

  /* edge case: file shorter than header */
  test_fail_if (i_truncate (&fp, PAGE_SIZE - 1, &e));
  struct file_pager pager;
  test_err_t_check (fpgr_open (&pager, "test.db", &e), ERR_CORRUPT, &e);

  /* edge case: file size = half a page */
  test_fail_if (i_truncate (&fp, PAGE_SIZE / 2, &e));
  test_err_t_check (fpgr_open (&pager, "test.db", &e), ERR_CORRUPT, &e);

  /* happy path: file exactly header size, zero pages */
  test_fail_if (i_truncate (&fp, 0, &e));
  test_err_t_check (fpgr_open (&pager, "test.db", &e), SUCCESS, &e);
  test_assert_int_equal ((int)pager.npages, 0);
  test_fail_if (fpgr_close (&pager, &e));

  /* happy path: file exactly header size, more pages */
  test_fail_if (i_truncate (&fp, 3 * PAGE_SIZE, &e));
  test_err_t_check (fpgr_open (&pager, "test.db", &e), SUCCESS, &e);
  test_assert_equal (pager.npages, 3);
  test_fail_if (fpgr_close (&pager, &e));

  /* There were 2 references to file - close it here too */
  test_fail_if (i_close (&fp, &e));
  test_fail_if (i_unlink ("test.db", &e));
}
#endif

err_t
fpgr_close (struct file_pager *f, error *e)
{
  DBG_ASSERT (file_pager, f);
  i_close (&f->f, e);
  return e->cause_code;
}

err_t
fpgr_reset (struct file_pager *f, error *e)
{
  DBG_ASSERT (file_pager, f);
  err_t_wrap (i_truncate (&f->f, 0, e), e);
  f->npages = 0;
  return e->cause_code;
}

p_size
fpgr_get_npages (const struct file_pager *fp)
{
  DBG_ASSERT (file_pager, fp);
  return fp->npages;
}

err_t
fpgr_new (struct file_pager *p, pgno *dest, error *e)
{
  DBG_ASSERT (file_pager, p);
  ASSERT (dest);

  i_log_trace ("File pager creating a new page\n");

  /* Extend the file by 1 page */
  err_t_wrap (i_truncate (&p->f, PAGE_SIZE * (1 + p->npages), e), e);

  *dest = p->npages++;

  i_log_trace ("File pager new total pages: %" PRpgno "\n", p->npages);

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, fpgr_new)
{
  i_file fp;
  error e = error_create ();
  test_fail_if (i_open_rw (&fp, "test.db", &e));

  test_fail_if (i_truncate (&fp, 0, &e));

  struct file_pager pager;
  test_err_t_check (fpgr_open (&pager, "test.db", &e), SUCCESS, &e);

  pgno pg = 0;

  /* Create a new page */
  test_fail_if (fpgr_new (&pager, &pg, &e));

  /* Page should be at position 0 */
  test_assert_int_equal (pg, 0);

  /* There should be 1 page */
  test_assert_int_equal (pager.npages, 1);

  /* Size should be PAGE_SIZE */
  test_assert_int_equal (i_file_size (&fp, &e), PAGE_SIZE * pager.npages);

  /* Add two more pages and do the same thing */
  test_fail_if (fpgr_new (&pager, &pg, &e));
  test_assert_int_equal (pg, 1);
  test_assert_int_equal (pager.npages, 2);
  test_assert_int_equal (i_file_size (&fp, &e), PAGE_SIZE * pager.npages);

  test_fail_if (fpgr_new (&pager, &pg, &e));
  test_assert_int_equal (pg, 2);
  test_assert_int_equal (pager.npages, 3);
  test_assert_int_equal (i_file_size (&fp, &e), PAGE_SIZE * pager.npages);

  test_fail_if (fpgr_close (&pager, &e));

  /* There were 2 references to file - close it here too */
  test_fail_if (i_close (&fp, &e));
  test_fail_if (i_unlink ("test.db", &e));
}
#endif

err_t
fpgr_read (struct file_pager *p, u8 *dest, pgno pg, error *e)
{
  DBG_ASSERT (file_pager, p);
  ASSERT (dest);
  if (pg >= p->npages)
    {
      return error_causef (e, ERR_PG_OUT_OF_RANGE,
                           "File Pager: Invalid page index. "
                           "Got page read: %" PRpgno " but total "
                           "amount of pages is %" PRpgno,
                           pg, p->npages);
    }

  /* Read all from file */
  i64 nread = i_pread_all (&p->f, dest, PAGE_SIZE, pg * PAGE_SIZE, e);

  if (nread == 0)
    {
      return error_causef (e, ERR_CORRUPT, "File pager: empty read");
    }

  if (nread < 0)
    {
      return e->cause_code;
    }

  return SUCCESS;
}

err_t
fpgr_delete (struct file_pager *p, pgno pg __attribute__ ((unused)), error *e)
{
  TODO ("Implement file pager delete");
  (void)e;
  DBG_ASSERT (file_pager, p);
  return SUCCESS;
}

err_t
fpgr_write (struct file_pager *p, const u8 *src, pgno pg, error *e)
{
  DBG_ASSERT (file_pager, p);
  ASSERT (src);
  ASSERT (pg < p->npages);

  err_t_wrap (i_pwrite_all (&p->f, src, PAGE_SIZE, pg * PAGE_SIZE, e), e);

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, fpgr_read_write)
{
  /* The raw page bytes */
  u8 _page[PAGE_SIZE];

  /* Create a temporary file */
  i_file fp;
  error e = error_create ();
  test_fail_if (i_open_rw (&fp, "test.db", &e));

  /* File should be size 0 */
  test_fail_if (i_truncate (&fp, 0, &e));

  /* Open a new pager */
  struct file_pager pager;
  fpgr_open (&pager, "test.db", &e);
  test_assert_int_equal (e.cause_code, SUCCESS);
  test_fail_if_null (&pager);

  /* happy path: new page, write, then read back */
  pgno pg = 0;
  test_fail_if (fpgr_new (&pager, &pg, &e));

  /* Write 0 : PAGE_SIZE to each byte (overflow fine, it's just data) */
  for (u32 i = 0; i < PAGE_SIZE; i++)
    {
      _page[i] = (u8)i;
    }
  /* Write it out */
  test_fail_if (fpgr_write (&pager, _page, pg, &e));

  /* Scramble page so we can read it back in */
  i_memset (_page, 0xFF, PAGE_SIZE);
  test_fail_if (fpgr_read (&pager, _page, pg, &e));

  /* Iterate and check that it matches what we expect */
  for (u32 i = 0; i < PAGE_SIZE; i++)
    {
      test_assert_int_equal (_page[i], (u8)i);
    }

  /* There's 2 references to this file, close the other one */
  test_fail_if (fpgr_close (&pager, &e));
  test_fail_if (i_close (&fp, &e));
  test_fail_if (i_unlink ("test.db", &e));
}
#endif

#ifndef NTEST
err_t
fpgr_crash (struct file_pager *p, error *e)
{
  DBG_ASSERT (file_pager, p);
  i_close (&p->f, e);
  return e->cause_code;
}
#endif
