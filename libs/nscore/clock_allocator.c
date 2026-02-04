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
 *   Implements clock_allocator.h. Clock-based page replacement allocator.
 */

#include <numstore/core/clock_allocator.h>

#include <numstore/core/cbuffer.h>
#include <numstore/core/error.h>
#include <numstore/core/random.h>
#include <numstore/test/testing.h>

err_t
clck_alloc_open (struct clck_alloc *ca, size_t elem_size, u32 nelems, error *e)
{
  ASSERT (ca);
  ASSERT (elem_size > 0);
  ASSERT (nelems > 0);

  ca->elem_size = elem_size;
  ca->nelems = nelems;
  ca->clock = 0;

  size_t data_size = nelems * elem_size;
  size_t occupied_size = nelems * sizeof (bool);
  size_t total_size = data_size + occupied_size;

  ca->data = i_malloc (1, total_size, e);
  if (ca->data == NULL)
    {
      return error_change_causef (e, ERR_NOMEM, "Failed to allocate clock allocator");
    }

  latch_init (&ca->l);

  i_memset (ca->data, 0, total_size);

  ca->occupied = (bool *)((char *)ca->data + data_size);

  return SUCCESS;
}

void *
clck_alloc_alloc (struct clck_alloc *ca, error *e)
{
  ASSERT (ca);
  ASSERT (ca->data);
  ASSERT (ca->occupied);

  latch_lock (&ca->l);
  for (u32 i = 0; i < ca->nelems; ++i)
    {
      u32 next = ca->clock;
      ca->clock = (ca->clock + 1) % ca->nelems;

      if (!ca->occupied[next])
        {
          ca->occupied[next] = true;
          void *ret = (char *)ca->data + (next * ca->elem_size);
          latch_unlock (&ca->l);
          return ret;
        }
    }
  latch_unlock (&ca->l);

  error_causef (e, ERR_NOMEM, "All clock allocator spots are full");
  return NULL;
}

void *
clck_alloc_calloc (struct clck_alloc *ca, error *e)
{
  void *ret = clck_alloc_alloc (ca, e);
  if (ret == NULL)
    {
      return ret;
    }

  i_memset (ret, 0, ca->elem_size);

  return ret;
}

void
clck_alloc_free (struct clck_alloc *ca, void *ptr)
{
  latch_lock (&ca->l);
  ASSERT (ca);
  ASSERT (ca->data);
  ASSERT (ca->occupied);
  ASSERT (ptr);

  // Calculate the index of this block
  ptrdiff_t offset = (char *)ptr - (char *)ca->data;
  ASSERT (offset >= 0);
  ASSERT ((size_t)offset < ca->nelems * ca->elem_size);
  ASSERT (offset % ca->elem_size == 0);

  u32 index = (u32)(offset / ca->elem_size);
  ASSERT (ca->occupied[index]);
  ca->occupied[index] = false;
  latch_unlock (&ca->l);
}

void
clck_alloc_close (struct clck_alloc *ca)
{
  ASSERT (ca);

  i_free (ca->data);
  ca->data = NULL;
  ca->occupied = NULL;
}

TEST (TT_UNIT, clck_alloc)
{
  error e = error_create ();
  struct clck_alloc ca;
  const u32 nelems = 100;

  err_t result = clck_alloc_open (&ca, sizeof (i32), nelems, &e);
  test_assert (result == SUCCESS);
  test_assert (ca.data != NULL);
  test_assert (ca.occupied != NULL);

  i32 *indices[nelems];

  // Allocate all slots
  for (u32 i = 0; i < nelems; ++i)
    {
      indices[i] = (i32 *)clck_alloc_alloc (&ca, &e);
      test_assert (indices[i] != NULL);
      *indices[i] = i;
    }

  // Try to allocate when full
  i32 *bad = (i32 *)clck_alloc_alloc (&ca, &e);
  test_assert (bad == NULL);
  e.cause_code = SUCCESS;

  // Free two slots
  clck_alloc_free (&ca, indices[10]);
  clck_alloc_free (&ca, indices[20]);

  // Reallocate them
  indices[10] = (i32 *)clck_alloc_alloc (&ca, &e);
  test_assert (indices[10] != NULL);

  indices[20] = (i32 *)clck_alloc_alloc (&ca, &e);
  test_assert (indices[20] != NULL);

  // Try to allocate when full again
  bad = (i32 *)clck_alloc_alloc (&ca, &e);
  test_assert (bad == NULL);
  e.cause_code = SUCCESS;

  // Free all slots
  for (u32 i = 0; i < nelems; ++i)
    {
      clck_alloc_free (&ca, indices[i]);
    }

  // Reallocate all slots
  for (u32 i = 0; i < nelems; ++i)
    {
      indices[i] = (i32 *)clck_alloc_alloc (&ca, &e);
      test_assert (indices[i] != NULL);
      *indices[i] = i;
    }

  // Free all slots
  for (u32 i = 0; i < nelems; ++i)
    {
      clck_alloc_free (&ca, indices[i]);
    }

  clck_alloc_close (&ca);
}

RANDOM_TEST (TT_UNIT, clock_allocator_random, 1)
{
  error e = error_create ();
  struct clck_alloc ca;

  err_t result = clck_alloc_open (&ca, sizeof (i32), 100, &e);
  test_assert (result == SUCCESS);
  test_assert (ca.data != NULL);
  test_assert (ca.occupied != NULL);

  i32 *_ptrs[100];
  i32 _values[100];

  struct cbuffer ptrs = cbuffer_create (_ptrs, sizeof (_ptrs));
  struct cbuffer values = cbuffer_create (_values, sizeof (_values));

  TEST_AGENT (1, "clock allocator")
  {
    bool alloc = randu32r (0, 2);
    bool full = false;

    if (cbuffer_avail (&ptrs) == 0)
      {
        alloc = false;
        full = true;
      }
    else if (cbuffer_len (&ptrs) == 0)
      {
        alloc = true;
      }

    if (alloc)
      {
        i32 *ptr = clck_alloc_alloc (&ca, &e);
        test_err_t_wrap (e.cause_code, &e);

        *ptr = randu32 ();

        cbuffer_write_expect (&ptr, sizeof (ptr), 1, &ptrs);
        cbuffer_write_expect (ptr, sizeof (*ptr), 1, &values);
      }
    else
      {
        if (full)
          {
            i32 *ptr = clck_alloc_alloc (&ca, &e);
            test_err_t_check (e.cause_code, ERR_NOMEM, &e);
          }

        i32 *ptr;
        i32 value;
        cbuffer_read_expect (&ptr, sizeof (ptr), 1, &ptrs);
        cbuffer_read_expect (&value, sizeof (value), 1, &values);

        test_assert_int_equal (*ptr, value);

        clck_alloc_free (&ca, ptr);
      }
  }

  clck_alloc_close (&ca);
}
