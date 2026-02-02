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
 *   Implements chunk_alloc.h. Fixed-size chunk allocator for efficient memory pooling.
 */

#include <numstore/core/chunk_alloc.h>

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/core/lalloc.h>

// Add to header
bool chunk_alloc_is_valid (const struct chunk_alloc *ca);
bool chunk_is_valid (const struct chunk *c);

// Implementation
DEFINE_DBG_ASSERT (struct chunk, chunk, c, {
  ASSERT (c);
  ASSERT (c->alloc.data == c->data);
  ASSERT (c->alloc.used <= c->alloc.limit);
  ASSERT (c->alloc.limit > 0);
  ASSERT (c->alloc.data == (u8 *)c + sizeof (struct chunk));
})

DEFINE_DBG_ASSERT (struct chunk_alloc, chunk_alloc, ca, {
  ASSERT (ca);
  ASSERT (ca->settings.target_chunk_mult >= 1.0f);
  ASSERT (ca->settings.min_chunk_size > 0);
  ASSERT (ca->settings.max_chunk_size == 0 || ca->settings.max_chunk_size >= ca->settings.min_chunk_size);
  ASSERT (ca->head != NULL || ca->num_chunks == 0);
  ASSERT (ca->total_used <= ca->total_allocated);
  ASSERT (ca->settings.max_total_size == 0 || ca->total_allocated <= ca->settings.max_total_size);
  ASSERT (ca->settings.max_chunks == 0 || ca->num_chunks < ca->settings.max_chunks);

  u32 counted_chunks = 0;
  u32 counted_allocated = 0;
  u32 counted_used = 0;

  for (const struct chunk *c = ca->head; c != NULL; c = c->next)
    {
      DBG_ASSERT (chunk, c);

      counted_chunks++;
      counted_allocated += c->alloc.limit;
      counted_used += c->alloc.used;

      ASSERT (counted_chunks <= ca->settings.max_chunks || ca->settings.max_chunks == 0);
      ASSERT (counted_chunks <= 10000);
    }

  // Verify counts match
  ASSERT (counted_chunks == ca->num_chunks);
  ASSERT (counted_allocated == ca->total_allocated);
  ASSERT (counted_used == ca->total_used);
})

static struct chunk *
chunk_create (u32 size, error *e)
{
  struct chunk *ret = i_malloc (sizeof (struct chunk) + size, 1, e);
  if (ret == NULL)
    {
      return NULL;
    }
  ret->alloc = lalloc_create (ret->data, size);
  ret->next = NULL;
  DBG_ASSERT (chunk, ret);
  return ret;
}

void
chunk_alloc_create_default (struct chunk_alloc *dest)
{
  chunk_alloc_create (dest, (struct chunk_alloc_settings){
                                .max_alloc_size = 0,
                                .max_total_size = 0,
                                .target_chunk_mult = 10,
                                .min_chunk_size = 10,
                                .max_chunk_size = 0,
                                .max_chunks = 0,
                            });
}

static inline u32
compute_new_chunk_size (struct chunk_alloc *ca, u32 alloc_size)
{
  DBG_ASSERT (chunk_alloc, ca);

  // Target chunk size based on multiplier
  u32 new_chunk_size = (u32)(alloc_size * ca->settings.target_chunk_mult);

  // Clamp to minimum
  if (new_chunk_size < ca->settings.min_chunk_size)
    {
      new_chunk_size = ca->settings.min_chunk_size;
    }

  // Clamp to maximum
  if (ca->settings.max_chunk_size > 0 && new_chunk_size > ca->settings.max_chunk_size)
    {
      new_chunk_size = ca->settings.max_chunk_size;
    }

  // Ensure it fits the current allocation
  if (new_chunk_size < alloc_size)
    {
      new_chunk_size = alloc_size;
    }

  return new_chunk_size;
}

void
chunk_alloc_create (struct chunk_alloc *dest, struct chunk_alloc_settings settings)
{

  ASSERT (settings.target_chunk_mult >= 1.0f);
  ASSERT (settings.min_chunk_size > 0);
  ASSERT (settings.max_chunk_size == 0 || settings.max_chunk_size >= settings.min_chunk_size);

  *dest = (struct chunk_alloc){
    .settings = settings,
    .head = NULL,
    .num_chunks = 0,
    .total_allocated = 0,
    .total_used = 0,
  };

  latch_init (&dest->latch);

  DBG_ASSERT (chunk_alloc, dest);
}

void
chunk_alloc_free_all (struct chunk_alloc *ca)
{
  latch_lock (&ca->latch);

  DBG_ASSERT (chunk_alloc, ca);

  struct chunk *cur = ca->head;
  while (cur != NULL)
    {
      struct chunk *next = cur->next;
      DBG_ASSERT (chunk, cur);
      i_free (cur);
      cur = next;
    }

  ca->head = NULL;
  ca->num_chunks = 0;
  ca->total_allocated = 0;
  ca->total_used = 0;

  latch_unlock (&ca->latch);
}

void
chunk_alloc_reset_all (struct chunk_alloc *ca)
{
  latch_lock (&ca->latch);

  DBG_ASSERT (chunk_alloc, ca);

  for (struct chunk *c = ca->head; c != NULL; c = c->next)
    {
      DBG_ASSERT (chunk, c);
      lalloc_reset (&c->alloc);
    }

  ca->total_used = 0;

  latch_unlock (&ca->latch);
}

static err_t
chunk_alloc_add_new_chunk (struct chunk_alloc *ca, u32 size, error *e)
{
  DBG_ASSERT (chunk_alloc, ca);

  // Check chunk count limit
  if (ca->settings.max_chunks > 0 && ca->num_chunks >= ca->settings.max_chunks)
    {
      return error_causef (
          e, ERR_NOMEM,
          "Cannot allocate chunk: limit is %u chunks, currently have %u",
          ca->settings.max_chunks, ca->num_chunks);
    }

  // Verify size constraints (internal assertions)
  ASSERT (size >= ca->settings.min_chunk_size);
  ASSERT (ca->settings.max_chunk_size == 0 || size <= ca->settings.max_chunk_size);

  // Check total memory limit
  if (ca->settings.max_total_size > 0)
    {
      if (ca->total_allocated + size > ca->settings.max_total_size)
        {
          return error_causef (
              e, ERR_NOMEM,
              "Cannot allocate %u byte chunk: would exceed total limit of %u bytes (%u currently allocated)",
              size, ca->settings.max_total_size, ca->total_allocated);
        }
    }

  // Create chunk
  struct chunk *new_chunk = chunk_create (size, e);
  if (new_chunk == NULL)
    {
      return e->cause_code;
    }

  // Add to front of list
  new_chunk->next = ca->head;
  ca->head = new_chunk;
  ca->num_chunks++;
  ca->total_allocated += size;

  return SUCCESS;
}

void *
chunk_malloc (struct chunk_alloc *ca, u32 req, u32 size, error *e)
{
  latch_lock (&ca->latch);

  DBG_ASSERT (chunk_alloc, ca);

  // Check for overflow
  if (req > 0 && size > UINT32_MAX / req)
    {
      error_causef (e, ERR_NOMEM,
                    "Allocation overflow: %u * %u exceeds maximum size",
                    req, size);
      latch_unlock (&ca->latch);
      return NULL;
    }

  u32 alloc_size = req * size;

  // Check single allocation limit
  if (ca->settings.max_alloc_size > 0 && alloc_size > ca->settings.max_alloc_size)
    {
      error_causef (
          e, ERR_NOMEM,
          "Allocation of %u bytes exceeds maximum allowed size of %u bytes",
          alloc_size, ca->settings.max_alloc_size);
      latch_unlock (&ca->latch);
      return NULL;
    }

  // Try current chunk first
  if (ca->head != NULL)
    {
      void *ptr = lmalloc (&ca->head->alloc, req, size, NULL);
      if (ptr != NULL)
        {
          ca->total_used += alloc_size;
          latch_unlock (&ca->latch);
          return ptr;
        }
    }

  // Need a new chunk - calculate size
  u32 new_chunk_size = compute_new_chunk_size (ca, alloc_size);

  // Create new chunk
  if (chunk_alloc_add_new_chunk (ca, new_chunk_size, e) != SUCCESS)
    {
      latch_unlock (&ca->latch);
      return NULL;
    }

  // Allocate from new chunk
  void *ptr = lmalloc (&ca->head->alloc, req, size, e);
  if (ptr != NULL)
    {
      ca->total_used += alloc_size;
    }

  latch_unlock (&ca->latch);

  return ptr;
}

void *
chunk_calloc (struct chunk_alloc *ca, u32 req, u32 size, error *e)
{
  DBG_ASSERT (chunk_alloc, ca);

  void *ptr = chunk_malloc (ca, req, size, e);
  if (ptr != NULL)
    {
      i_memset (ptr, 0, req * size);
    }
  return ptr;
}

void *
chunk_alloc_move_mem (struct chunk_alloc *ca, void *ptr, u32 size, error *e)
{
  void *dest = chunk_malloc (ca, size, 1, e);

  if (dest == NULL)
    {
      return NULL;
    }

  i_memcpy (dest, ptr, size);

  return dest;
}
