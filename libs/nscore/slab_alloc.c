#include "numstore/core/assert.h"
#include <numstore/core/slab_alloc.h>

struct slab
{
  void *freelist;
  struct slab *next;
  struct slab *prev;
  u32 used;
  u32 total_size;
  u8 data[];
};

void
slab_alloc_init (struct slab_alloc *dest, u32 size, u32 cap_per_slab)
{
  ASSERT (size >= sizeof (void *));
  ASSERT (cap_per_slab > 0);

  // Align size to pointer boundary for better performance
  size = (size + sizeof (void *) - 1) & ~(sizeof (void *) - 1);

  *dest = (struct slab_alloc){
    .head = NULL,
    .current = NULL,
    .size = size,
    .cap_per_slab = cap_per_slab,
  };
  latch_init (&dest->l);
}

void
slab_alloc_destroy (struct slab_alloc *alloc)
{
  ASSERT (alloc);

  struct slab *s = alloc->head;
  while (s)
    {
      struct slab *next = s->next;
      i_free (s);
      s = next;
    }

  alloc->head = NULL;
  alloc->current = NULL;
}

static struct slab *
slab_alloc_extend (struct slab_alloc *alloc, error *e)
{
  u32 data_size = alloc->size * alloc->cap_per_slab;
  u32 total_size = data_size + sizeof (struct slab);

  struct slab *slab = i_malloc (1, total_size, e);
  if (slab == NULL)
    {
      return NULL;
    }

  slab->total_size = total_size;

  // Link at head
  slab->prev = NULL;
  slab->next = alloc->head;
  if (alloc->head)
    {
      ASSERT (alloc->head->prev == NULL);
      alloc->head->prev = slab;
    }
  alloc->head = slab;
  slab->used = 0;

  // Initialize freelist
  slab->freelist = slab->data;
  u8 *cur = slab->data;
  for (u32 i = 0; i < alloc->cap_per_slab - 1; ++i)
    {
      u8 *next = cur + alloc->size;
      *(void **)cur = next;
      cur = next;
    }
  *(void **)cur = NULL;

  return slab;
}

void *
slab_alloc_alloc (struct slab_alloc *alloc, error *e)
{
  ASSERT (alloc);

  void *ret = NULL;
  latch_lock (&alloc->l);

  struct slab *s = alloc->current;

  // HOT PATH: Try cached current slab first
  if (s && s->freelist)
    {
      ret = s->freelist;
      s->freelist = *(void **)ret;
      s->used++;
      goto theend;
    }

  // SLOW PATH: Find or create slab with space
  s = alloc->head;
  while (s && !s->freelist)
    {
      s = s->next;
    }

  if (!s)
    {
      s = slab_alloc_extend (alloc, e);
      if (s == NULL)
        {
          goto theend;
        }
    }

  // Update cache
  alloc->current = s;

  ret = s->freelist;
  s->freelist = *(void **)ret;
  s->used++;

theend:
  latch_unlock (&alloc->l);
  return ret;
}

static inline bool
slab_contains (struct slab_alloc *alloc, struct slab *s, void *ptr)
{
  void *start = s->data;
  void *end = (u8 *)start + (alloc->cap_per_slab * alloc->size);
  return ptr >= start && ptr < end;
}

static inline struct slab *
slab_from_ptr (struct slab_alloc *alloc, void *ptr)
{
  // Try the cached slab
  struct slab *s = alloc->current;
  if (s && slab_contains (alloc, s, ptr))
    {
      return s;
    }

  // Try remaining slabs
  s = alloc->head;
  while (s)
    {
      if (slab_contains (alloc, s, ptr))
        {
          return s;
        }
      s = s->next;
    }

  UNREACHABLE ();
}

void
slab_alloc_free (struct slab_alloc *alloc, void *ptr)
{
  ASSERT (alloc);
  ASSERT (ptr);

  latch_lock (&alloc->l);

  struct slab *s = slab_from_ptr (alloc, ptr);
  ASSERT (s);

  // Add to freelist
  ASSERT (s->used > 0);
  *(void **)ptr = s->freelist;
  s->freelist = ptr;
  s->used--;

  // Update current cache if this slab now has space
  if (alloc->current == NULL || alloc->current->freelist == NULL)
    {
      alloc->current = s;
    }

  // Free empty slabs
  if (s->used == 0)
    {
      if (s->next || s->prev)
        {
          // Clear cache if we're freeing it
          if (alloc->current == s)
            {
              alloc->current = NULL;
            }

          // Update head if we're freeing it
          if (s == alloc->head)
            {
              alloc->head = s->next;
            }

          if (s->prev)
            {
              s->prev->next = s->next;
            }
          if (s->next)
            {
              s->next->prev = s->prev;
            }

          i_free (s);
        }
    }

  latch_unlock (&alloc->l);
}

#ifndef NTEST
#include <numstore/core/random.h>
#include <numstore/test/testing.h>

struct test_item
{
  i32 a;
  u64 b;
  char data[10];
};

static void
test_item_init (struct test_item *item, i32 value)
{
  item->a = value;
  item->b = (u64)value * 1000;
  for (int i = 0; i < 10; i++)
    {
      item->data[i] = (char)(value + i);
    }
}

static void
test_item_verify (struct test_item *item, i32 expected)
{
  test_assert_equal (item->a, expected);
  test_assert_equal (item->b, (u64)expected * 1000);
  for (int i = 0; i < 10; i++)
    {
      test_assert_equal (item->data[i], (char)(expected + i));
    }
}

TEST (TT_UNIT, slab_alloc_simple)
{
  struct slab_alloc alloc;
  error e = error_create ();

  slab_alloc_init (&alloc, sizeof (struct test_item), 5);

  // Allocate 20 items (will span 4 slabs)
  struct test_item *items[20];
  for (int i = 0; i < 20; i++)
    {
      items[i] = slab_alloc_alloc (&alloc, &e);
      test_assert (items[i] != NULL);
      test_item_init (items[i], i);
    }

  // Verify all items
  for (int i = 0; i < 20; i++)
    {
      test_item_verify (items[i], i);
    }

  // Free every other item (indices 0, 2, 4, ... 18)
  for (int i = 0; i < 20; i += 2)
    {
      slab_alloc_free (&alloc, items[i]);
      items[i] = NULL;
    }

  // Verify remaining items (indices 1, 3, 5, ... 19)
  for (int i = 1; i < 20; i += 2)
    {
      test_item_verify (items[i], i);
    }

  // Allocate 10 new items (should reuse freed slots)
  struct test_item *new_items[10];
  for (int i = 0; i < 10; i++)
    {
      new_items[i] = slab_alloc_alloc (&alloc, &e);
      test_assert (new_items[i] != NULL);
      test_item_init (new_items[i], 100 + i);
    }

  // Verify old items still intact
  for (int i = 1; i < 20; i += 2)
    {
      test_item_verify (items[i], i);
    }

  // Verify new items
  for (int i = 0; i < 10; i++)
    {
      test_item_verify (new_items[i], 100 + i);
    }

  // Free first half of new items (indices 0-4)
  for (int i = 0; i < 5; i++)
    {
      slab_alloc_free (&alloc, new_items[i]);
      new_items[i] = NULL;
    }

  // Verify remaining new items (indices 5-9)
  for (int i = 5; i < 10; i++)
    {
      test_item_verify (new_items[i], 100 + i);
    }

  // Verify old items still intact
  for (int i = 1; i < 20; i += 2)
    {
      test_item_verify (items[i], i);
    }

  // Allocate another batch
  struct test_item *batch3[15];
  for (int i = 0; i < 15; i++)
    {
      batch3[i] = slab_alloc_alloc (&alloc, &e);
      test_assert (batch3[i] != NULL);
      test_item_init (batch3[i], 200 + i);
    }

  // Verify all three batches
  for (int i = 1; i < 20; i += 2)
    {
      test_item_verify (items[i], i);
    }
  for (int i = 5; i < 10; i++)
    {
      test_item_verify (new_items[i], 100 + i);
    }
  for (int i = 0; i < 15; i++)
    {
      test_item_verify (batch3[i], 200 + i);
    }

  // Free everything
  for (int i = 1; i < 20; i += 2)
    {
      slab_alloc_free (&alloc, items[i]);
    }
  for (int i = 5; i < 10; i++)
    {
      slab_alloc_free (&alloc, new_items[i]);
    }
  for (int i = 0; i < 15; i++)
    {
      slab_alloc_free (&alloc, batch3[i]);
    }

  slab_alloc_destroy (&alloc);
}

#endif
