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
 *   Implements rptree_mem.h. Provides an in-memory implementation of R+ tree
 *   operations for testing and comparison, using a simple byte array backend.
 */

#include <rptree_mem.h>

#include <numstore/core/assert.h>
#include <numstore/core/cbuffer.h>
#include <numstore/core/error.h>
#include <numstore/core/math.h>
#include <numstore/core/random.h>
#include <numstore/intf/os.h>
#include <numstore/test/testing.h>

// core
DEFINE_DBG_ASSERT (struct rptree_mem, rptree_mem, r, {
  ASSERT (r);
  ASSERT (r->vlen <= r->vcap);
  ASSERT (r->view);
})

struct rptree_mem *
rptm_new (error *e)
{
  struct rptree_mem *ret = i_malloc (1, sizeof *ret, e);
  if (ret == NULL)
    {
      return NULL;
    }

  ret->view = i_malloc (1, 1000, e);
  if (ret->view == NULL)
    {
      i_free (ret);
      return NULL;
    }

  ret->vcap = 1000;
  ret->vlen = 0;

  if (latch_init (&ret->latch, e) < SUCCESS)
    {
      i_free (ret->view);
      i_free (ret);
      return NULL;
    }

  return ret;
}

void
rptm_close (struct rptree_mem *r)
{
  DBG_ASSERT (rptree_mem, r);
  i_free (r->view);
  i_free (r);
}

///////////////////////////////////////////
/// ONE OFF

b_size
rptm_read (struct rptree_mem *r, struct rptrd params)
{
  latch_lock (&r->latch);

  ASSERT (params.attr.stride > 0);

  const b_size nbytes = params.bsize * params.attr.nelems;
  const b_size bofst = params.bsize * params.attr.ofst;

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
      ASSERT (bleft % params.bsize == 0 && bleft > 0 && bavail > 0);

      switch (state)
        {
        case RPTM_READING:
          {
            // Check if we have enough bytes for a complete element
            if (srci + params.bsize > r->vlen)
              {
                goto done; // Partial read - stop here
              }

            i_memcpy ((u8 *)params.dest + desti, &r->view[srci], params.bsize);
            desti += params.bsize;
            srci += params.bsize;

            state = RPTM_SKIPPING;
            break;
          }
        case RPTM_SKIPPING:
          {
            srci += MIN ((params.attr.stride - 1) * params.bsize, bleft); // FIX: was bavail
            state = RPTM_READING;
            break;
          }
        }
    }

done:
  {
    b_size result = desti / params.bsize;
    latch_unlock (&r->latch);
    return result;
  }
}

b_size
rptm_remove (struct rptree_mem *r, struct rptrm params)
{
  latch_lock (&r->latch);

  DBG_ASSERT (rptree_mem, r);
  ASSERT (params.attr.stride > 0);

  const b_size nbytes = params.bsize * params.attr.nelems;
  const b_size bofst = params.bsize * params.attr.ofst;

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
      ASSERT (bleft % params.bsize == 0 && bleft > 0 && bavail > 0);

      switch (state)
        {
        case RPTM_REMOVING:
          {
            // Check if we have enough bytes for a complete element
            if (srci + params.bsize > r->vlen)
              {
                goto cleanup; // Partial element - stop removing
              }

            if (params.dest != NULL)
              {
                i_memcpy ((u8 *)params.dest + desti, &r->view[srci], params.bsize);
              }
            desti += params.bsize;
            srci += params.bsize;
            state = RPTM_KEEPING;
            break;
          }
        case RPTM_KEEPING:
          {
            b_size to_keep = MIN ((params.attr.stride - 1) * params.bsize, bleft);
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

  b_size result = desti / params.bsize;

  latch_unlock (&r->latch);

  return result;
}

b_size
rptm_write (struct rptree_mem *r, struct rptw params)
{
  DBG_ASSERT (rptree_mem, r);
  ASSERT (params.attr.stride > 0);

  latch_lock (&r->latch);

  const b_size nbytes = params.bsize * params.attr.nelems;
  const b_size bofst = params.bsize * params.attr.ofst;

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
      ASSERT (bleft % params.bsize == 0 && bleft > 0 && bavail > 0);

      switch (state)
        {
        case RPTM_WRITING:
          {
            // Check if we have enough space for a complete element
            if (desti + params.bsize > r->vlen)
              {
                goto done; // Partial write - stop here
              }

            i_memcpy (&r->view[desti], (const u8 *)params.src + srci, params.bsize);
            srci += params.bsize;
            desti += params.bsize;
            state = RPTM_SKIPPING;
            break;
          }
        case RPTM_SKIPPING:
          {
            desti += MIN ((params.attr.stride - 1) * params.bsize, bleft);
            state = RPTM_WRITING;
            break;
          }
        }
    }

done:
  {
    b_size result = srci / params.bsize;
    latch_unlock (&r->latch);
    return result;
  }
}

err_t
rptm_insert (struct rptree_mem *r, struct rpti params, error *e)
{
  latch_lock (&r->latch);

  const b_size nbytes = params.bsize * params.attr.nelems;
  b_size bofst = params.bsize * params.attr.ofst;

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

  i_memcpy (&r->view[bofst], params.src, nbytes);

  if (right_half)
    {
      i_memcpy (&r->view[bofst + nbytes], right_half, r->vlen - bofst);
      i_free (right_half);
    }

  r->vlen = r->vlen + nbytes;

  latch_unlock (&r->latch);

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, rptm_basic)
{
  TEST_CASE ("Insert and read simple")
  {
    error e = error_create ();
    struct rptree_mem *r = rptm_new (&e);

    u8 data[100];
    rand_bytes (data, sizeof (data));

    // Insert 100 bytes (100 elements of size 1)
    test_err_t_wrap (rptm_insert (
                         r, (struct rpti){
                                .src = data,
                                .bsize = 1,
                                .attr = {
                                    .nelems = 100,
                                    .ofst = 0,
                                },
                            },
                         &e),
                     &e);
    test_assert (r->vlen == 100);
    test_assert_memequal (r->view, data, 100);

    // Read back
    u8 read_buf[100];

    rptm_read (
        r, (struct rptrd){
               .dest = read_buf,
               .bsize = 1,
               .attr = {
                   .nelems = 100,
                   .ofst = 0,
                   .stride = 1,
               },
           });
    test_assert_memequal (read_buf, data, 100);

    rptm_close (r);
  }

  TEST_CASE ("Insert with element offset")
  {
    error e = error_create ();
    struct rptree_mem *r = rptm_new (&e);

    u8 data1[50];
    u8 data2[50];
    rand_bytes (data1, sizeof (data1));
    rand_bytes (data2, sizeof (data2));

    // Insert 50 bytes at offset 0 (elements of size 1)
    test_err_t_wrap (rptm_insert (
                         r, (struct rpti){
                                .src = data1,
                                .bsize = 1,
                                .attr = {
                                    .nelems = 50,
                                    .ofst = 0,
                                },
                            },
                         &e),
                     &e);

    // Insert 50 bytes at element offset 50 (byte offset 50)
    test_err_t_wrap (rptm_insert (
                         r, (struct rpti){
                                .src = data2,
                                .bsize = 1,
                                .attr = {
                                    .nelems = 50,
                                    .ofst = 50,
                                },
                            },
                         &e),
                     &e);

    test_assert (r->vlen == 100);
    test_assert_memequal (r->view, data1, 50);
    test_assert_memequal (&r->view[50], data2, 50);

    rptm_close (r);
  }

  TEST_CASE ("Read with element offset")
  {
    error e = error_create ();
    struct rptree_mem *r = rptm_new (&e);

    u8 data[100];
    arr_range (data);
    test_err_t_wrap (rptm_insert (
                         r, (struct rpti){
                                .src = data,
                                .bsize = 1,
                                .attr = {
                                    .nelems = 100,
                                    .ofst = 0,
                                },
                            },
                         &e),
                     &e);

    // Read 50 bytes starting at element offset 25 (byte offset 25)
    u8 read_buf[50];
    rptm_read (
        r, (struct rptrd){
               .dest = read_buf,
               .bsize = 1,
               .attr = {
                   .nelems = 50,
                   .ofst = 25,
                   .stride = 1,
               },
           });
    test_assert_memequal (read_buf, &data[25], 50);

    rptm_close (r);
  }

  TEST_CASE ("Read with stride")
  {
    error e = error_create ();
    struct rptree_mem *r = rptm_new (&e);

    u8 data[200];
    arr_range (data);
    test_err_t_wrap (rptm_insert (
                         r, (struct rpti){
                                .src = data,
                                .bsize = 1,
                                .attr = {
                                    .nelems = 200,
                                    .ofst = 0,
                                },
                            },
                         &e),
                     &e);

    // Read with stride=2, bsize=10: read 10, skip 10, read 10, skip 10...
    u8 read_buf[80];
    rptm_read (
        r, (struct rptrd){
               .dest = read_buf,
               .bsize = 10,
               .attr = {
                   .nelems = 8,
                   .ofst = 0,
                   .stride = 2,
               },
           });

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
    struct rptree_mem *r = rptm_new (&e);

    u32 data[25]; // 100 bytes
    for (u32 i = 0; i < 25; ++i)
      {
        data[i] = i;
      }

    // Insert 25 elements of size 4 at offset 0
    test_err_t_wrap (rptm_insert (
                         r, (struct rpti){
                                .src = data,
                                .bsize = 4,
                                .attr = {
                                    .nelems = 25,
                                    .ofst = 0,
                                },
                            },
                         &e),
                     &e);
    test_assert (r->vlen == 100);
    test_assert_memequal (r->view, data, 100);

    rptm_close (r);
  }

  TEST_CASE ("Read multi-byte elements with offset")
  {
    error e = error_create ();
    struct rptree_mem *r = rptm_new (&e);

    u32 data[50]; // 200 bytes
    for (u32 i = 0; i < 50; ++i)
      {
        data[i] = i;
      }

    test_err_t_wrap (rptm_insert (
                         r, (struct rpti){
                                .src = data,
                                .bsize = 4,
                                .attr = {
                                    .nelems = 50,
                                    .ofst = 0,
                                },
                            },
                         &e),
                     &e);

    // Read 10 elements of size 4 starting at element offset 20 (byte offset 80)
    u32 read_buf[10];
    rptm_read (
        r, (struct rptrd){
               .dest = read_buf,
               .bsize = 4,
               .attr = {
                   .nelems = 10,
                   .ofst = 20,
                   .stride = 1,
               },
           });

    test_assert_memequal (read_buf, &data[20], 40);

    rptm_close (r);
  }
}
#endif
