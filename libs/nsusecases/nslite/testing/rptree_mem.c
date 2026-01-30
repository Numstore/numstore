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
 *   Implements rptree_segment.h. Provides an in-memory implementation of R+ tree
 *   operations for testing and comparison, using a simple byte array backend.
 */

#include <numstore/test/rptree_mem.h>

#include <numstore/core/assert.h>
#include <numstore/core/cbuffer.h>
#include <numstore/core/error.h>
#include <numstore/core/math.h>
#include <numstore/core/random.h>
#include <numstore/intf/os.h>
#include <numstore/test/testing.h>

// core
DEFINE_DBG_ASSERT (struct rptree_segment, rptree_segment, r, {
  ASSERT (r);
  ASSERT (r->vlen <= r->vcap);
  ASSERT (r->view);
})

static err_t
rpts_open (struct rptree_segment *dest, error *e)
{
  dest->view = i_malloc (1, 1000, e);
  if (dest->view == NULL)
    {
      return e->cause_code;
    }

  dest->vcap = 1000;
  dest->vlen = 0;

  latch_init (&dest->latch);

  return SUCCESS;
}

static void
rpts_close (struct rptree_segment *r)
{
  DBG_ASSERT (rptree_segment, r);
  i_free (r->view);
}

struct rptree_mem *
rptm_open (error *e)
{
  struct rptree_mem *ret = i_malloc (1, sizeof *ret, e);
  if (ret == NULL)
    {
      return NULL;
    }

  slab_alloc_init (&ret->alloc, sizeof (struct rptree_segment), 256);

  ht_init_rptm (&ret->pgno_to_segment, ret->_hdata, arrlen (ret->_hdata));

  latch_init (&ret->l);

  return ret;
}

void
rptm_close (struct rptree_mem *r)
{
  latch_lock (&r->l);

  for (u32 i = 0; i < arrlen (r->_hdata); ++i)
    {
      if (r->_hdata[i].present)
        {
          hdata_rptm data;
          ht_delete_expect_rptm (&r->pgno_to_segment, &data, r->_hdata[i].data.key);
          rpts_close (data.value);
        }
    }
  slab_alloc_destroy (&r->alloc);

  latch_unlock (&r->l);

  i_free (r);
}

///////////////////////////////////////////
/// ONE OFF

err_t
rptm_new (struct rptree_mem *n, pgno pg, error *e)
{
  struct rptree_segment *rpts = slab_alloc_alloc (&n->alloc, e);
  if (rpts == NULL)
    {
      return e->cause_code;
    }

  if (rpts_open (rpts, e))
    {
      slab_alloc_free (&n->alloc, rpts);
      return e->cause_code;
    }

  hdata_rptm data = {
    .key = pg,
    .value = rpts,
  };

  ht_insert_expect_rptm (&n->pgno_to_segment, data);

  return SUCCESS;
}

void
rptm_delete (struct rptree_mem *n, pgno start)
{
  hdata_rptm dest;
  ht_delete_expect_rptm (&n->pgno_to_segment, &dest, start);
  rpts_close (dest.value);
}

static inline b_size
rpts_size (struct rptree_segment *n)
{
  return n->vlen;
}

b_size
rptm_size (struct rptree_mem *n, pgno id)
{
  hdata_rptm dest;
  ht_get_expect_rptm (&n->pgno_to_segment, &dest, id);
  return rpts_size (dest.value);
}

static b_size
rpts_read (struct rptree_segment *r, void *dest, t_size size, struct rptm_stride stride)
{
  latch_lock (&r->latch);

  ASSERT (stride.stride > 0);

  const b_size nbytes = size * stride.nelems;
  const b_size bofst = stride.bstart;

  b_size srci = bofst;
  b_size desti = 0;

  enum
  {
    RPTM_READING,
    RPTM_SKIPPING,
  } state
      = RPTM_READING;

  while (srci < r->vlen && desti < nbytes)
    {
      b_size bavail = nbytes - desti;
      b_size bleft = r->vlen - srci;
      ASSERT (bleft % size == 0 && bleft > 0 && bavail > 0);

      switch (state)
        {
        case RPTM_READING:
          {
            // Check if we have enough bytes for a complete element
            if (srci + size > r->vlen)
              {
                goto done; // Partial read - stop here
              }

            i_memcpy ((u8 *)dest + desti, &r->view[srci], size);
            desti += size;
            srci += size;

            state = RPTM_SKIPPING;
            break;
          }
        case RPTM_SKIPPING:
          {
            srci += MIN ((stride.stride - 1) * size, bleft); // FIX: was bavail
            state = RPTM_READING;
            break;
          }
        }
    }

done:
  {
    b_size result = desti / size;
    latch_unlock (&r->latch);
    return result;
  }
}

b_size
rptm_read (struct rptree_mem *r, pgno id, void *dest, t_size size, struct rptm_stride stride)
{
  hdata_rptm data;
  ht_get_expect_rptm (&r->pgno_to_segment, &data, id);
  return rpts_read (data.value, dest, size, stride);
}

static b_size
rpts_remove (struct rptree_segment *r, void *dest, t_size size, struct rptm_stride stride)
{
  latch_lock (&r->latch);

  DBG_ASSERT (rptree_segment, r);
  ASSERT (stride.stride > 0);

  const b_size nbytes = size * stride.nelems;
  const b_size bofst = stride.bstart;

  if (bofst >= r->vlen)
    {
      latch_unlock (&r->latch);
      return 0;
    }

  b_size srci = bofst;
  b_size desti = 0;
  b_size compacti = bofst;

  enum
  {
    RPTM_REMOVING,
    RPTM_KEEPING,
  } state
      = RPTM_REMOVING;

  while (srci < r->vlen && desti < nbytes)
    {
      b_size bavail = nbytes - desti;
      b_size bleft = r->vlen - srci;
      ASSERT (bleft % size == 0 && bleft > 0 && bavail > 0);

      switch (state)
        {
        case RPTM_REMOVING:
          {
            // Check if we have enough bytes for a complete element
            if (srci + size > r->vlen)
              {
                goto cleanup; // Partial element - stop removing
              }

            if (dest != NULL)
              {
                i_memcpy ((u8 *)dest + desti, &r->view[srci], size);
              }
            desti += size;
            srci += size;
            state = RPTM_KEEPING;
            break;
          }
        case RPTM_KEEPING:
          {
            b_size to_keep = MIN ((stride.stride - 1) * size, bleft);
            i_memmove (&r->view[compacti], &r->view[srci], to_keep);
            compacti += to_keep;
            srci += to_keep;
            state = RPTM_REMOVING;
            break;
          }
        }
    }

cleanup:
  if (srci < r->vlen)
    {
      i_memmove (&r->view[compacti], &r->view[srci], r->vlen - srci);
      compacti += r->vlen - srci;
    }

  r->vlen = compacti;

  b_size result = desti / size;

  latch_unlock (&r->latch);

  return result;
}

b_size
rptm_remove (struct rptree_mem *r, pgno id, void *dest, t_size size, struct rptm_stride stride)
{
  hdata_rptm data;
  ht_get_expect_rptm (&r->pgno_to_segment, &data, id);
  return rpts_remove (data.value, dest, size, stride);
}

static b_size
rpts_write (struct rptree_segment *r, const void *src, t_size size, struct rptm_stride stride)
{
  DBG_ASSERT (rptree_segment, r);
  ASSERT (stride.stride > 0);

  latch_lock (&r->latch);

  const b_size nbytes = size * stride.nelems;
  const b_size bofst = stride.bstart;

  if (bofst >= r->vlen)
    {
      latch_unlock (&r->latch);
      return 0;
    }

  b_size srci = 0;
  b_size desti = bofst;

  enum
  {
    RPTM_WRITING,
    RPTM_SKIPPING,
  } state
      = RPTM_WRITING;

  while (desti < r->vlen && srci < nbytes)
    {
      b_size bavail = nbytes - srci;
      b_size bleft = r->vlen - desti;
      ASSERT (bleft % size == 0 && bleft > 0 && bavail > 0);

      switch (state)
        {
        case RPTM_WRITING:
          {
            // Check if we have enough space for a complete element
            if (desti + size > r->vlen)
              {
                goto done; // Partial write - stop here
              }

            i_memcpy (&r->view[desti], (const u8 *)src + srci, size);
            srci += size;
            desti += size;
            state = RPTM_SKIPPING;
            break;
          }
        case RPTM_SKIPPING:
          {
            desti += MIN ((stride.stride - 1) * size, bleft);
            state = RPTM_WRITING;
            break;
          }
        }
    }

done:
  {
    b_size result = srci / size;
    latch_unlock (&r->latch);
    return result;
  }
}

b_size
rptm_write (struct rptree_mem *r, pgno id, const void *src, t_size size, struct rptm_stride stride)
{
  hdata_rptm data;
  ht_get_expect_rptm (&r->pgno_to_segment, &data, id);
  return rpts_write (data.value, src, size, stride);
}

static err_t
rpts_insert (struct rptree_segment *r, const void *src, b_size bofst, t_size size, b_size nelem, error *e)
{
  latch_lock (&r->latch);

  const b_size nbytes = size * nelem;

  if (bofst >= r->vlen)
    {
      bofst = r->vlen;
    }

  u32 target_len = r->vlen + nbytes;
  u8 *right_half = NULL;

  if (bofst < r->vlen)
    {
      right_half = i_malloc (r->vlen - bofst, 1, e);
      if (right_half == NULL)
        {
          latch_unlock (&r->latch);
          return e->cause_code;
        }
    }

  if (target_len > r->vcap)
    {
      u8 *view = i_realloc_right (r->view, r->vcap, target_len * 2, 1, e);
      if (view == NULL)
        {
          i_free (right_half);
          latch_unlock (&r->latch);
          return e->cause_code;
        }
      r->view = view;
      r->vcap = target_len * 2;
    }

  if (right_half)
    {
      i_memcpy (right_half, &r->view[bofst], r->vlen - bofst);
    }

  i_memcpy (&r->view[bofst], src, nbytes);

  if (right_half)
    {
      i_memcpy (&r->view[bofst + nbytes], right_half, r->vlen - bofst);
      i_free (right_half);
    }

  r->vlen = r->vlen + nbytes;

  latch_unlock (&r->latch);

  return SUCCESS;
}

err_t
rptm_insert (struct rptree_mem *r, pgno id, const void *src, b_size bofst, t_size size, b_size nelem, error *e)
{
  hdata_rptm data;
  ht_get_expect_rptm (&r->pgno_to_segment, &data, id);
  return rpts_insert (data.value, src, bofst, size, nelem, e);
}

#ifndef NTEST
TEST (TT_UNIT, rptm_basic)
{
  TEST_CASE ("Insert and read simple")
  {
    error e = error_create ();
    struct rptree_mem *r = rptm_open (&e);
    test_err_t_wrap (rptm_new (r, 0, &e), &e);

    u8 data[100];
    rand_bytes (data, sizeof (data));

    // Insert 100 bytes (100 elements of size 1)
    test_err_t_wrap (rptm_insert (r, 0, data, 0, 1, 100, &e), &e);
    test_assert (rptm_size (r, 0) == 100);

    // Read back
    u8 read_buf[100];
    b_size read_len = rptm_read (
        r, 0, read_buf, 1,
        (struct rptm_stride){
            .bstart = 0,
            .stride = 1,
            .nelems = 100,
        });
    test_assert (read_len == 100);
    test_assert_memequal (read_buf, data, 100);

    rptm_close (r);
  }

  TEST_CASE ("Insert with byte offset")
  {
    error e = error_create ();
    struct rptree_mem *r = rptm_open (&e);
    test_err_t_wrap (rptm_new (r, 0, &e), &e);

    u8 data1[50];
    u8 data2[50];
    rand_bytes (data1, sizeof (data1));
    rand_bytes (data2, sizeof (data2));

    // Insert 50 bytes at byte offset 0
    test_err_t_wrap (rptm_insert (r, 0, data1, 0, 1, 50, &e), &e);

    // Insert 50 bytes at byte offset 50
    test_err_t_wrap (rptm_insert (r, 0, data2, 50, 1, 50, &e), &e);

    test_assert (rptm_size (r, 0) == 100);

    // Read back both sections
    u8 read_buf[100];
    b_size read_len = rptm_read (
        r, 0, read_buf, 1,
        (struct rptm_stride){
            .bstart = 0,
            .stride = 1,
            .nelems = 100,
        });
    test_assert (read_len == 100);
    test_assert_memequal (read_buf, data1, 50);
    test_assert_memequal (&read_buf[50], data2, 50);

    rptm_close (r);
  }

  TEST_CASE ("Read with byte offset")
  {
    error e = error_create ();
    struct rptree_mem *r = rptm_open (&e);
    test_err_t_wrap (rptm_new (r, 0, &e), &e);

    u8 data[100];
    arr_range (data);
    test_err_t_wrap (rptm_insert (r, 0, data, 0, 1, 100, &e), &e);

    // Read 50 bytes starting at byte offset 25
    u8 read_buf[50];
    b_size read_len = rptm_read (
        r, 0, read_buf, 1,
        (struct rptm_stride){
            .bstart = 25,
            .stride = 1,
            .nelems = 50,
        });
    test_assert (read_len == 50);
    test_assert_memequal (read_buf, &data[25], 50);

    rptm_close (r);
  }

  TEST_CASE ("Read with stride")
  {
    error e = error_create ();
    struct rptree_mem *r = rptm_open (&e);
    test_err_t_wrap (rptm_new (r, 0, &e), &e);

    u8 data[200];
    arr_range (data);
    test_err_t_wrap (rptm_insert (r, 0, data, 0, 1, 200, &e), &e);

    // Read with stride=2, size=10: read 10, skip 10, read 10, skip 10...
    u8 read_buf[80];
    b_size read_len = rptm_read (
        r, 0, read_buf, 10,
        (struct rptm_stride){
            .bstart = 0,
            .stride = 2,
            .nelems = 8,
        });
    test_assert_int_equal (read_len, 8);

    // Verify: should get bytes 0-9, 20-29, 40-49, 60-69, 80-89, 100-109, 120-129, 140-149
    for (u32 i = 0; i < 8; ++i)
      {
        test_assert_memequal (&read_buf[i * 10], &data[i * 20], 10);
      }

    rptm_close (r);
  }

  TEST_CASE ("Insert multi-byte elements")
  {
    error e = error_create ();
    struct rptree_mem *r = rptm_open (&e);
    test_err_t_wrap (rptm_new (r, 0, &e), &e);

    u32 data[25]; // 100 bytes
    for (u32 i = 0; i < 25; ++i)
      {
        data[i] = i;
      }

    // Insert 25 elements of size 4 at byte offset 0
    test_err_t_wrap (rptm_insert (r, 0, data, 0, 4, 25, &e), &e);
    test_assert (rptm_size (r, 0) == 100);

    // Read back
    u32 read_buf[25];
    b_size read_len = rptm_read (
        r, 0, read_buf, 4,
        (struct rptm_stride){
            .bstart = 0,
            .stride = 1,
            .nelems = 25,
        });
    test_assert_int_equal (read_len, 25);
    test_assert_memequal (read_buf, data, 100);

    rptm_close (r);
  }

  TEST_CASE ("Read multi-byte elements with offset")
  {
    error e = error_create ();
    struct rptree_mem *r = rptm_open (&e);
    test_err_t_wrap (rptm_new (r, 0, &e), &e);

    u32 data[50]; // 200 bytes
    for (u32 i = 0; i < 50; ++i)
      {
        data[i] = i;
      }

    test_err_t_wrap (rptm_insert (r, 0, data, 0, 4, 50, &e), &e);

    // Read 10 elements of size 4 starting at byte offset 80 (element 20)
    u32 read_buf[10];
    b_size read_len = rptm_read (
        r, 0, read_buf, 4,
        (struct rptm_stride){
            .bstart = 80,
            .stride = 1,
            .nelems = 10,
        });
    test_assert_int_equal (read_len, 10);
    test_assert_memequal (read_buf, &data[20], 40);

    rptm_close (r);
  }
}
#endif
