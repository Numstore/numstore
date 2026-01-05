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
 *   Implements node_updates.h. Provides functions for managing node update
 *   structures during tree modifications, tracking changes to be propagated
 *   up the tree during rebalancing.
 */

#include <numstore/rptree/node_updates.h>

#include <numstore/core/assert.h>
#include <numstore/pager/inner_node.h>
#include <numstore/pager/page.h>
#include <numstore/test/testing.h>

// numstore
DEFINE_DBG_ASSERT (
    struct node_updates, node_updates, n,
    {
      ASSERT (n);
      ASSERT (!in_pair_is_empty (n->pivot));

      ASSERT (n->rlen <= arrlen (n->right));
      ASSERT (n->llen <= arrlen (n->left));

      // obs can only be > len on the last node
      ASSERT (n->robs <= arrlen (n->right));
      ASSERT (n->lobs <= arrlen (n->left));

      /**
       * This is the main feature
       * You can't consume a node unless you've
       * observed it
       */
      ASSERT (n->rcons <= n->robs);
      ASSERT (n->lcons <= n->lobs);
    })

struct in_pair *
nupd_init (struct node_updates *s, pgno pg, b_size size)
{
  i_memset (s, 0, sizeof *s);
  s->pivot = in_pair_from (pg, size);

  DBG_ASSERT (node_updates, s);

  s->prev = &s->pivot;

  return &s->pivot;
}

#ifndef NTEST
TEST (TT_UNIT, nupd_init)
{
  TEST_CASE ("Initialize with page and size")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    test_assert_equal (n.pivot.pg, 100);
    test_assert_equal (n.pivot.key, 512);
    test_assert_equal (n.llen, 0);
    test_assert_equal (n.rlen, 0);
    test_assert_equal (n.lobs, 0);
    test_assert_equal (n.robs, 0);
    test_assert_equal (n.lcons, 0);
    test_assert_equal (n.rcons, 0);
  }

  TEST_CASE ("Initialize with zero values")
  {
    struct node_updates n;
    nupd_init (&n, 0, 0);

    test_assert_equal (n.pivot.pg, 0);
    test_assert_equal (n.pivot.key, 0);
  }

  TEST_CASE ("Initialize with large values")
  {
    struct node_updates n;
    nupd_init (&n, 999999, 65536);

    test_assert_equal (n.pivot.pg, 999999);
    test_assert_equal (n.pivot.key, 65536);
  }
}

#endif

static struct in_pair *
nupd_append_right (struct node_updates *s, pgno pg, b_size size)
{
  DBG_ASSERT (node_updates, s);
  ASSERT (s->robs == 0);
  ASSERT (s->lobs == 0);
  ASSERT (s->rcons == 0);
  ASSERT (s->lcons == 0);

  struct in_pair *ret;

  if (s->rlen == 0 && pg == s->pivot.pg)
    {
      ret = &s->pivot;
      s->pivot.key = size;
    }
  else
    {
      ret = &s->right[s->rlen];
      s->right[s->rlen++] = in_pair_from (pg, size);
    }

  return ret;
}

#ifndef NTEST
TEST (TT_UNIT, nupd_append_right)
{
  TEST_CASE ("Append single right entry")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    struct in_pair *ret = nupd_append_right (&n, 200, 1024);

    test_assert_equal (n.rlen, 1);
    test_assert_equal (n.right[0].pg, 200);
    test_assert_equal (n.right[0].key, 1024);
    test_assert_equal (ret, &n.right[0]);
  }

  TEST_CASE ("Append to pivot when page matches")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    struct in_pair *ret = nupd_append_right (&n, 100, 1024);

    test_assert_equal (n.rlen, 0);
    test_assert_equal (n.pivot.pg, 100);
    test_assert_equal (n.pivot.key, 1024);
    test_assert_equal (ret, &n.pivot);
  }

  TEST_CASE ("Append multiple entries")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    nupd_append_right (&n, 200, 1024);
    nupd_append_right (&n, 300, 2048);
    nupd_append_right (&n, 400, 4096);

    test_assert_equal (n.rlen, 3);
    test_assert_equal (n.right[0].pg, 200);
    test_assert_equal (n.right[1].pg, 300);
    test_assert_equal (n.right[2].pg, 400);
    test_assert_equal (n.right[0].key, 1024);
    test_assert_equal (n.right[1].key, 2048);
    test_assert_equal (n.right[2].key, 4096);
  }

  TEST_CASE ("Append after pivot update")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    nupd_append_right (&n, 100, 1024); // Updates pivot
    nupd_append_right (&n, 200, 2048); // Should add to array

    test_assert_equal (n.rlen, 1);
    test_assert_equal (n.pivot.key, 1024);
    test_assert_equal (n.right[0].pg, 200);
  }
}
#endif

static struct in_pair *
nupd_append_left (struct node_updates *s, pgno pg, b_size size)
{
  DBG_ASSERT (node_updates, s);
  ASSERT (s->robs == 0);
  ASSERT (s->lobs == 0);
  ASSERT (s->rcons == 0);
  ASSERT (s->lcons == 0);

  struct in_pair *ret;

  if (s->llen == 0 && pg == s->pivot.pg)
    {
      ret = &s->pivot;
      s->pivot.key = size;
    }
  else
    {
      ret = &s->left[s->llen];
      s->left[s->llen++] = in_pair_from (pg, size);
    }

  return ret;
}

#ifndef NTEST
TEST (TT_UNIT, nupd_append_left)
{
  TEST_CASE ("Append single left entry")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    struct in_pair *ret = nupd_append_left (&n, 50, 256);

    test_assert_equal (n.llen, 1);
    test_assert_equal (n.left[0].pg, 50);
    test_assert_equal (n.left[0].key, 256);
    test_assert_equal (ret, &n.left[0]);
  }

  TEST_CASE ("Append to pivot when page matches")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    struct in_pair *ret = nupd_append_left (&n, 100, 768);

    test_assert_equal (n.llen, 0);
    test_assert_equal (n.pivot.pg, 100);
    test_assert_equal (n.pivot.key, 768);
    test_assert_equal (ret, &n.pivot);
  }

  TEST_CASE ("Append multiple entries")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    nupd_append_left (&n, 90, 256);
    nupd_append_left (&n, 80, 128);
    nupd_append_left (&n, 70, 64);

    test_assert_equal (n.llen, 3);
    test_assert_equal (n.left[0].pg, 90);
    test_assert_equal (n.left[1].pg, 80);
    test_assert_equal (n.left[2].pg, 70);
  }

  TEST_CASE ("Append after pivot update")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    nupd_append_left (&n, 100, 256); // Updates pivot
    nupd_append_left (&n, 50, 128);  // Should add to array

    test_assert_equal (n.llen, 1);
    test_assert_equal (n.pivot.key, 256);
    test_assert_equal (n.left[0].pg, 50);
  }
}
#endif

void
nupd_commit_1st_right (struct node_updates *s, pgno pg, b_size size)
{
  if (s->prev != NULL)
    {
      ASSERT (s->prev->pg == pg);
      s->prev->key = size;
      s->prev = NULL;
    }
  else
    {
      nupd_append_right (s, pg, size);
    }
}

void
nupd_commit_1st_left (struct node_updates *s, pgno pg, b_size size)
{
  if (s->prev != NULL)
    {
      ASSERT (s->prev->pg == pg);
      s->prev->key = size;
      s->prev = NULL;
    }
  else
    {
      nupd_append_left (s, pg, size);
    }
}

void
nupd_append_2nd_right (struct node_updates *s, pgno pg1, b_size size1, pgno pg2, b_size size2)
{
  if (s->prev != NULL)
    {
      ASSERT (s->prev->pg == pg1);
    }
  else
    {
      s->prev = nupd_append_right (s, pg1, size1);
    }
  nupd_append_right (s, pg2, size2);
}

void
nupd_append_2nd_left (struct node_updates *s, pgno pg1, b_size size1, pgno pg2, b_size size2)
{
  if (s->prev != NULL)
    {
      ASSERT (s->prev->pg == pg1);
    }
  else
    {
      s->prev = nupd_append_left (s, pg1, size1);
    }
  nupd_append_left (s, pg2, size2);
}

void
nupd_append_tip_right (struct node_updates *s, struct three_in_pair output)
{
  nupd_commit_1st_right (s, output.cur.pg, output.cur.key);

  if (!in_pair_is_empty (output.prev))
    {
      for (u32 i = s->rlen; i > 0; --i)
        {
          if (s->right[i - 1].pg == output.prev.pg)
            {
              s->right[i - 1].key = output.prev.key;
              goto regular;
            }
        }

      if (s->pivot.pg == output.prev.pg)
        {
          s->pivot.key = output.prev.key;
          goto regular;
        }

      for (u32 i = 0; i < s->llen; ++i)
        {
          if (s->left[i].pg == output.prev.pg)
            {
              s->left[i].key = output.prev.key;
              goto regular;
            }
        }

      nupd_append_left (s, output.prev.pg, output.prev.key);
    }

regular:

  if (!in_pair_is_empty (output.next))
    {
      nupd_append_right (s, output.next.pg, output.next.key);
    }
}

#ifndef NTEST
TEST (TT_UNIT, nupd_append_tip_right)
{
  TEST_CASE ("Append with only current")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    struct three_in_pair tip = {
      .prev = in_pair_empty,
      .cur = { .pg = 200, .key = 1024 },
      .next = in_pair_empty,
    };

    nupd_commit_1st_right (&n, 100, 512);
    nupd_append_tip_right (&n, tip);

    test_assert_equal (n.rlen, 1);
    test_assert_equal (n.right[0].pg, 200);
    test_assert_equal (n.right[0].key, 1024);
    test_assert_equal (n.llen, 0);
  }

  TEST_CASE ("Append with current and next")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    struct three_in_pair tip = {
      .prev = in_pair_empty,
      .cur = { .pg = 200, .key = 1024 },
      .next = { .pg = 300, .key = 2048 }
    };

    nupd_commit_1st_right (&n, 100, 512);
    nupd_append_tip_right (&n, tip);

    test_assert_equal (n.rlen, 2);
    test_assert_equal (n.right[0].pg, 200);
    test_assert_equal (n.right[1].pg, 300);
    test_assert_equal (n.llen, 0);
  }

  TEST_CASE ("Append with all three pairs")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    struct three_in_pair tip = {
      .prev = { .pg = 150, .key = 768 },
      .cur = { .pg = 200, .key = 1024 },
      .next = { .pg = 300, .key = 2048 }
    };

    nupd_commit_1st_right (&n, 100, 512);
    nupd_append_tip_right (&n, tip);

    test_assert_equal (n.rlen, 2);
    test_assert_equal (n.right[0].pg, 200);
    test_assert_equal (n.right[1].pg, 300);
    test_assert_equal (n.llen, 1);
    test_assert_equal (n.left[0].pg, 150);
  }

  TEST_CASE ("Update existing prev in right array")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    nupd_commit_1st_right (&n, 100, 512);
    nupd_commit_1st_right (&n, 150, 512);

    struct three_in_pair tip = {
      .prev = { .pg = 150, .key = 999 },
      .cur = { .pg = 200, .key = 1024 },
      .next = in_pair_empty,
    };

    nupd_append_tip_right (&n, tip);

    test_assert_equal (n.rlen, 2);
    test_assert_equal (n.right[0].pg, 150);
    test_assert_equal (n.right[0].key, 999);
    test_assert_equal (n.llen, 0);
  }

  TEST_CASE ("Update pivot as prev")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    struct three_in_pair tip = {
      .prev = { .pg = 100, .key = 999 },
      .cur = { .pg = 200, .key = 1024 },
      .next = in_pair_empty,
    };

    nupd_commit_1st_right (&n, 100, 512);
    nupd_append_tip_right (&n, tip);

    test_assert_equal (n.pivot.key, 999);
    test_assert_equal (n.rlen, 1);
    test_assert_equal (n.llen, 0);
  }

  TEST_CASE ("Append prev when not found")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    nupd_commit_1st_right (&n, 100, 512);
    nupd_commit_1st_right (&n, 200, 1024);

    struct three_in_pair tip = {
      .prev = { .pg = 250, .key = 1536 },
      .cur = { .pg = 300, .key = 2048 },
      .next = in_pair_empty,
    };

    nupd_append_tip_right (&n, tip);

    test_assert_equal (n.rlen, 2);
    test_assert_equal (n.llen, 1);
    test_assert_equal (n.left[0].pg, 250);
  }
}
#endif

void
nupd_append_tip_left (struct node_updates *s, struct three_in_pair output)
{
  nupd_commit_1st_left (s, output.cur.pg, output.cur.key);

  if (!in_pair_is_empty (output.next))
    {
      for (u32 i = s->llen; i > 0; --i)
        {
          if (s->left[i - 1].pg == output.next.pg)
            {
              s->left[i - 1].key = output.next.key;
              goto regular;
            }
        }

      if (s->pivot.pg == output.next.pg)
        {
          s->pivot.key = output.next.key;
          goto regular;
        }

      for (u32 i = 0; i < s->rlen; ++i)
        {
          if (s->right[i].pg == output.next.pg)
            {
              s->right[i].key = output.next.key;
              goto regular;
            }
        }

      nupd_append_right (s, output.next.pg, output.next.key);
    }

regular:

  if (!in_pair_is_empty (output.prev))
    {
      nupd_append_left (s, output.prev.pg, output.prev.key);
    }
}

#ifndef NTEST
TEST (TT_UNIT, nupd_append_tip_left)
{
  TEST_CASE ("Append with only current")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    nupd_commit_1st_left (&n, 100, 512);

    struct three_in_pair tip = {
      .prev = in_pair_empty,
      .cur = { .pg = 50, .key = 256 },
      .next = in_pair_empty,
    };

    nupd_append_tip_left (&n, tip);

    test_assert_equal (n.llen, 1);
    test_assert_equal (n.left[0].pg, 50);
    test_assert_equal (n.left[0].key, 256);
    test_assert_equal (n.rlen, 0);
  }

  TEST_CASE ("Append with current and prev")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    nupd_commit_1st_left (&n, 100, 512);

    struct three_in_pair tip = {
      .prev = { .pg = 25, .key = 128 },
      .cur = { .pg = 50, .key = 256 },
      .next = in_pair_empty,
    };

    nupd_append_tip_left (&n, tip);

    test_assert_equal (n.llen, 2);
    test_assert_equal (n.left[0].pg, 50);
    test_assert_equal (n.left[1].pg, 25);
    test_assert_equal (n.rlen, 0);
  }

  TEST_CASE ("Append with all three pairs")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    nupd_commit_1st_left (&n, 100, 512);

    struct three_in_pair tip = {
      .next = { .pg = 75, .key = 384 },
      .cur = { .pg = 50, .key = 256 },
      .prev = { .pg = 25, .key = 128 }
    };

    nupd_append_tip_left (&n, tip);

    test_assert_equal (n.llen, 2);
    test_assert_equal (n.left[0].pg, 50);
    test_assert_equal (n.left[1].pg, 25);
    test_assert_equal (n.rlen, 1);
    test_assert_equal (n.right[0].pg, 75);
  }

  TEST_CASE ("Update existing next in left array")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    nupd_commit_1st_left (&n, 100, 512);
    nupd_commit_1st_left (&n, 75, 384);

    struct three_in_pair tip = {
      .next = { .pg = 75, .key = 999 },
      .cur = { .pg = 50, .key = 256 },
      .prev = in_pair_empty,
    };

    nupd_append_tip_left (&n, tip);

    test_assert_equal (n.llen, 2);
    test_assert_equal (n.left[0].pg, 75);
    test_assert_equal (n.left[0].key, 999);
  }

  TEST_CASE ("Update pivot as next")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    nupd_commit_1st_left (&n, 100, 512);

    struct three_in_pair tip = {
      .next = { .pg = 100, .key = 888 },
      .cur = { .pg = 50, .key = 256 },
      .prev = in_pair_empty,
    };

    nupd_append_tip_left (&n, tip);

    test_assert_equal (n.pivot.key, 888);
    test_assert_equal (n.llen, 1);
    test_assert_equal (n.rlen, 0);
  }
}
#endif

static void
nupd_observe_right (struct node_updates *s, pgno pg, b_size key)
{
  DBG_ASSERT (node_updates, s);

  if (s->robs == 0 && s->pivot.pg == pg)
    {
      return;
    }

  while (s->robs < s->rlen)
    {
      if (s->right[s->robs].pg == pg)
        {
          return;
        }
      s->robs++;
    }

  ASSERT (s->robs < arrlen (s->right));

  s->right[s->robs++] = in_pair_from (pg, key);
}

static void
nupd_observe_left (struct node_updates *s, pgno pg, b_size key)
{
  DBG_ASSERT (node_updates, s);

  if (s->lobs == 0 && s->pivot.pg == pg)
    {
      return;
    }

  while (s->lobs < s->llen)
    {
      if (s->left[s->lobs].pg == pg)
        {
          return;
        }
      s->lobs++;
    }

  ASSERT (s->lobs < arrlen (s->left));

  s->left[s->lobs++] = in_pair_from (pg, key);
}

void
nupd_observe_pivot (struct node_updates *s, page_h *pg, p_size lidx)
{
  DBG_ASSERT (node_updates, s);
  ASSERT (s->robs == 0);
  ASSERT (s->lobs == 0);
  ASSERT (s->rcons == 0);
  ASSERT (s->lcons == 0);
  ASSERT (page_h_type (pg) == PG_INNER_NODE);

  nupd_observe_right_from (s, pg, lidx);
  if (lidx > 0)
    {
      nupd_observe_left_from (s, pg, lidx);
    }
}

void
nupd_observe_right_from (struct node_updates *s, page_h *pg, p_size lidx)
{
  DBG_ASSERT (node_updates, s);
  ASSERT (s->robs <= s->rlen);

  if (pg->mode == PHM_NONE)
    {
      s->robs = s->rlen;
      return;
    }

  ASSERT (page_h_type (pg) == PG_INNER_NODE);

  for (p_size i = lidx; i < in_get_len (page_h_ro (pg)) && i < IN_MAX_KEYS; ++i)
    {
      pgno p = in_get_leaf (page_h_ro (pg), i);
      b_size b = in_get_key (page_h_ro (pg), i);
      nupd_observe_right (s, p, b);
    }
}

void
nupd_observe_left_from (struct node_updates *s, page_h *pg, p_size lidx)
{
  DBG_ASSERT (node_updates, s);
  ASSERT (s->lobs <= s->llen);

  if (pg->mode == PHM_NONE)
    {
      s->lobs = s->llen;
      return;
    }

  ASSERT (page_h_type (pg) == PG_INNER_NODE);

  for (p_size i = lidx; i > 0; --i)
    {
      pgno p = in_get_leaf (page_h_ro (pg), i - 1);
      b_size b = in_get_key (page_h_ro (pg), i - 1);
      nupd_observe_left (s, p, b);
    }
}

void
nupd_observe_all_right (struct node_updates *s, page_h *pg)
{
  DBG_ASSERT (node_updates, s);
  ASSERT (s->robs <= s->rlen);

  if (pg->mode == PHM_NONE)
    {
      s->robs = s->rlen;
      return;
    }

  ASSERT (page_h_type (pg) == PG_INNER_NODE);

  for (p_size i = 0; i < in_get_len (page_h_ro (pg)) && i < IN_MAX_KEYS; ++i)
    {
      pgno p = in_get_leaf (page_h_ro (pg), i);
      b_size b = in_get_key (page_h_ro (pg), i);
      nupd_observe_right (s, p, b);
    }
}

void
nupd_observe_all_left (struct node_updates *s, page_h *pg)
{
  DBG_ASSERT (node_updates, s);
  ASSERT (s->lobs <= s->llen);

  if (pg->mode == PHM_NONE)
    {
      s->lobs = s->llen;
      return;
    }

  ASSERT (page_h_type (pg) == PG_INNER_NODE);

  for (p_size i = in_get_len (page_h_ro (pg)); i > 0; --i)
    {
      pgno p = in_get_leaf (page_h_ro (pg), i - 1);
      b_size b = in_get_key (page_h_ro (pg), i - 1);
      nupd_observe_left (s, p, b);
    }
}

struct in_pair
nupd_consume_right (struct node_updates *s)
{
  DBG_ASSERT (node_updates, s);
  ASSERT (s->rcons < arrlen (s->right));

  return s->right[s->rcons++];
}

#ifndef NTEST
TEST (TT_UNIT, nupd_consume_right)
{
  TEST_CASE ("Consume single entry")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_right (&n, 200, 1024);
    n.robs = 1;

    struct in_pair p = nupd_consume_right (&n);

    test_assert_equal (p.pg, 200);
    test_assert_equal (p.key, 1024);
    test_assert_equal (n.rcons, 1);
  }

  TEST_CASE ("Consume multiple entries in order")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_right (&n, 200, 1024);
    nupd_append_right (&n, 300, 2048);
    nupd_append_right (&n, 400, 4096);
    n.robs = 3;

    struct in_pair p1 = nupd_consume_right (&n);
    test_assert_equal (p1.pg, 200);
    test_assert_equal (n.rcons, 1);

    struct in_pair p2 = nupd_consume_right (&n);
    test_assert_equal (p2.pg, 300);
    test_assert_equal (n.rcons, 2);

    struct in_pair p3 = nupd_consume_right (&n);
    test_assert_equal (p3.pg, 400);
    test_assert_equal (n.rcons, 3);
  }
}
#endif

struct in_pair
nupd_consume_left (struct node_updates *s)
{
  DBG_ASSERT (node_updates, s);
  ASSERT (s->lcons < arrlen (s->left));

  return s->left[s->lcons++];
}

#ifndef NTEST
TEST (TT_UNIT, nupd_consume_left)
{
  TEST_CASE ("Consume single entry")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_left (&n, 50, 256);
    n.lobs = 1;

    struct in_pair p = nupd_consume_left (&n);

    test_assert_equal (p.pg, 50);
    test_assert_equal (p.key, 256);
    test_assert_equal (n.lcons, 1);
  }

  TEST_CASE ("Consume multiple entries in order")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_left (&n, 90, 512);
    nupd_append_left (&n, 80, 256);
    nupd_append_left (&n, 70, 128);
    n.lobs = 3;

    struct in_pair p1 = nupd_consume_left (&n);
    test_assert_equal (p1.pg, 90);
    test_assert_equal (n.lcons, 1);

    struct in_pair p2 = nupd_consume_left (&n);
    test_assert_equal (p2.pg, 80);
    test_assert_equal (n.lcons, 2);

    struct in_pair p3 = nupd_consume_left (&n);
    test_assert_equal (p3.pg, 70);
    test_assert_equal (n.lcons, 3);
  }
}
#endif

bool
nupd_done_observing_left (struct node_updates *s)
{
  return s->lobs >= s->llen;
}

#ifndef NTEST
TEST (TT_UNIT, nupd_done_observing_left)
{
  TEST_CASE ("True when no left entries")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    test_assert_equal (nupd_done_observing_left (&n), 1);
  }

  TEST_CASE ("False when left entries not observed")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_left (&n, 50, 256);

    test_assert_equal (nupd_done_observing_left (&n), 0);
  }

  TEST_CASE ("True when all left entries observed")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_left (&n, 50, 256);
    nupd_append_left (&n, 25, 128);
    n.lobs = 2;

    test_assert_equal (nupd_done_observing_left (&n), 1);
  }

  TEST_CASE ("False when partially observed")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_left (&n, 50, 256);
    nupd_append_left (&n, 25, 128);
    n.lobs = 1;

    test_assert_equal (nupd_done_observing_left (&n), 0);
  }
}
#endif

bool
nupd_done_observing_right (struct node_updates *s)
{
  return s->robs >= s->rlen;
}

#ifndef NTEST
TEST (TT_UNIT, nupd_done_observing_right)
{
  TEST_CASE ("True when no right entries")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    test_assert_equal (nupd_done_observing_right (&n), 1);
  }

  TEST_CASE ("False when right entries not observed")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_right (&n, 200, 1024);

    test_assert_equal (nupd_done_observing_right (&n), 0);
  }

  TEST_CASE ("True when all right entries observed")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_right (&n, 200, 1024);
    nupd_append_right (&n, 300, 2048);
    n.robs = 2;

    test_assert_equal (nupd_done_observing_right (&n), 1);
  }
}
#endif

bool
nupd_done_consuming_left (struct node_updates *s)
{
  return s->lcons == s->lobs;
}

#ifndef NTEST
TEST (TT_UNIT, nupd_done_consuming_left)
{
  TEST_CASE ("True when nothing to consume")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    test_assert_equal (nupd_done_consuming_left (&n), 1);
  }

  TEST_CASE ("False when observed but not consumed")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_left (&n, 50, 256);
    n.lobs = 1;

    test_assert_equal (nupd_done_consuming_left (&n), 0);
  }

  TEST_CASE ("True when fully consumed")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_left (&n, 50, 256);
    n.lobs = 1;
    n.lcons = 1;

    test_assert_equal (nupd_done_consuming_left (&n), 1);
  }
}
#endif

bool
nupd_done_consuming_right (struct node_updates *s)
{
  return s->rcons == s->robs;
}

#ifndef NTEST
TEST (TT_UNIT, nupd_done_consuming_right)
{
  TEST_CASE ("True when nothing to consume")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    test_assert_equal (nupd_done_consuming_right (&n), 1);
  }

  TEST_CASE ("False when observed but not consumed")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_right (&n, 200, 1024);
    n.robs = 1;

    test_assert_equal (nupd_done_consuming_right (&n), 0);
  }

  TEST_CASE ("True when fully consumed")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_right (&n, 200, 1024);
    n.robs = 1;
    n.rcons = 1;

    test_assert_equal (nupd_done_consuming_right (&n), 1);
  }
}
#endif

bool
nupd_done_left (struct node_updates *s)
{
  return nupd_done_observing_left (s) && nupd_done_consuming_left (s);
}

#ifndef NTEST
TEST (TT_UNIT, nupd_done_left)
{
  TEST_CASE ("True for empty node")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    test_assert_equal (nupd_done_left (&n), 1);
  }

  TEST_CASE ("False when not observed")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_left (&n, 50, 256);

    test_assert_equal (nupd_done_left (&n), 0);
  }

  TEST_CASE ("False when observed but not consumed")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_left (&n, 50, 256);
    n.lobs = 1;

    test_assert_equal (nupd_done_left (&n), 0);
  }

  TEST_CASE ("True when observed and consumed")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_left (&n, 50, 256);
    n.lobs = 1;
    n.lcons = 1;

    test_assert_equal (nupd_done_left (&n), 1);
  }
}
#endif

bool
nupd_done_right (struct node_updates *s)
{
  return nupd_done_observing_right (s) && nupd_done_consuming_right (s);
}

#ifndef NTEST
TEST (TT_UNIT, nupd_done_right)
{
  TEST_CASE ("True for empty node")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);

    test_assert_equal (nupd_done_right (&n), 1);
  }

  TEST_CASE ("False when not observed")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_right (&n, 200, 1024);

    test_assert_equal (nupd_done_right (&n), 0);
  }

  TEST_CASE ("False when observed but not consumed")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_right (&n, 200, 1024);
    n.robs = 1;

    test_assert_equal (nupd_done_right (&n), 0);
  }

  TEST_CASE ("True when observed and consumed")
  {
    struct node_updates n;
    nupd_init (&n, 100, 512);
    nupd_append_right (&n, 200, 1024);
    n.robs = 1;
    n.rcons = 1;

    test_assert_equal (nupd_done_right (&n), 1);
  }
}
#endif

p_size
nupd_append_maximally_left (struct node_updates *n, page_h *pg, p_size lidx)
{
  DBG_ASSERT (node_updates, n);
  ASSERT (page_h_type (pg) == PG_INNER_NODE);
  ASSERT (in_get_len (page_h_ro (pg)) == IN_MAX_KEYS);

  p_size ret = 0;  // Total amount appended
  p_size i = lidx; // 1 + Location of append

  while (i > 0 && n->lcons < n->lobs)
    {
      struct in_pair next = nupd_consume_left (n);
      if (next.key > 0)
        {
          in_set_key_leaf (page_h_w (pg), i - 1, next.key, next.pg);
          i--;
          ret++;
        }
    }

  return ret;
}

p_size
nupd_append_maximally_right (struct node_updates *n, page_h *pg, p_size lidx)
{
  DBG_ASSERT (node_updates, n);
  ASSERT (page_h_type (pg) == PG_INNER_NODE);
  ASSERT (in_get_len (page_h_ro (pg)) == IN_MAX_KEYS);

  p_size ret = 0;  // Amount appended
  p_size i = lidx; // Location of append

  while (i < IN_MAX_KEYS && n->rcons < n->robs)
    {
      struct in_pair next = nupd_consume_right (n);
      if (next.key > 0)
        {
          in_set_key_leaf (page_h_w (pg), i, next.key, next.pg);
          i++;
          ret++;
        }
    }

  return ret;
}

p_size
nupd_append_maximally_left_then_right (struct node_updates *n, page_h *pg)
{
  DBG_ASSERT (node_updates, n);
  ASSERT (page_h_type (pg) == PG_INNER_NODE);
  ASSERT (in_get_len (page_h_ro (pg)) == IN_MAX_KEYS);

  /**
   * Append Pivot to the end
   *    [-------------------+]
   */
  p_size len = 0;
  if (n->pivot.key > 0)
    {
      in_set_key_leaf (page_h_w (pg), IN_MAX_KEYS - (++len), n->pivot.key, n->pivot.pg);
    }

  /**
   * Apply left
   *    [------++++++++++++++]
   */
  len += nupd_append_maximally_left (n, pg, IN_MAX_KEYS - len);

  ASSERT (len <= IN_MAX_KEYS);

  // Haven't finish - apply right remaining pages
  if (len < IN_MAX_KEYS)
    {
      /**
       * Shift left (memcpy x1)
       *    [++++++++++++++------]
       */
      in_cut_left (page_h_w (pg), IN_MAX_KEYS - len);

      /**
       * Apply right
       *    [++++++++++++++++++--]
       */
      len += nupd_append_maximally_right (n, pg, len);
    }

  return len;
}

p_size
nupd_append_maximally_right_then_left (struct node_updates *n, page_h *pg)
{
  DBG_ASSERT (node_updates, n);
  ASSERT (page_h_type (pg) == PG_INNER_NODE);
  ASSERT (in_get_len (page_h_ro (pg)) == IN_MAX_KEYS);

  /**
   * Append Pivot to the front
   *    [+-------------------]
   *      ^
   *    len = 1
   */
  p_size len = 0;
  if (n->pivot.key > 0)
    {
      in_set_key_leaf (page_h_w (pg), len++, n->pivot.key, n->pivot.pg);
    }

  /**
   * Apply right
   *    [++++++++++++++------]
   *                   ^
   *                  len
   */
  len += nupd_append_maximally_right (n, pg, len);

  ASSERT (len <= IN_MAX_KEYS);

  // Haven't finish - apply right remaining pages
  if (len < IN_MAX_KEYS)
    {
      /**
       * Shift right (memcpy x1)
       *    [------++++++++++++++]
       *          ^
       *    IN_MAX_KEYS - len - 1
       */
      in_push_left_permissive (page_h_w (pg), len);

      /**
       * Apply left
       *    [--++++++++++++++++++]
       */
      len += nupd_append_maximally_left (n, pg, IN_MAX_KEYS - len);
    }

  return len;
}
