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
 *   Implements page_fixture.h. Test fixture setup and teardown for page-related tests.
 */

#include <numstore/test/page_fixture.h>

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/core/random.h>
#include <numstore/pager.h>
#include <numstore/pager/data_list.h>
#include <numstore/pager/inner_node.h>
#include <numstore/pager/page.h>
#include <numstore/pager/page_delegate.h>
#include <numstore/pager/page_h.h>
#include <numstore/pager/pager_routines.h>
#include <numstore/test/testing.h>
#include <numstore/test/testing_test.h>

#ifndef NTEST
DEFINE_DBG_ASSERT (struct pgr_fixture, pgr_fixture, f, {
  ASSERT (f);
  ASSERT (f->p);
  ASSERT (f->e.cause_code == SUCCESS);
})

DEFINE_DBG_ASSERT (
    struct in_page_builder, in_page_builder, in,
    {
      ASSERT (in);
      ASSERT (in->dclen <= IN_MAX_KEYS);
      ASSERT (in->children.len <= in->dclen);
    })

DEFINE_DBG_ASSERT (
    struct dl_page_builder, dl_page_builder, in,
    {
      ASSERT (in);
      ASSERT (in->dclen <= DL_DATA_SIZE);
      ASSERT (in->data.blen <= in->dclen);
      if (in->data.blen == 0)
        {
          ASSERT (in->data.data == NULL);
        }
      else
        {
          ASSERT (in->data.data != NULL);
        }
    })

err_t
pgr_fixture_create (struct pgr_fixture *dest)
{
  ASSERT (dest);
  dest->e = error_create ();

  err_t_wrap (i_remove_quiet ("test.db", &dest->e), &dest->e);
  err_t_wrap (i_remove_quiet ("test.wal", &dest->e), &dest->e);

  err_t_wrap (lockt_init (&dest->lt, &dest->e), &dest->e);

  dest->tp = tp_open (&dest->e);
  if (dest->tp == NULL)
    {
      lockt_destroy (&dest->lt);
      return dest->e.cause_code;
    }

  struct pager *p = pgr_open ("test.db", "test.wal", &dest->lt, dest->tp, &dest->e);
  if (p == NULL)
    {
      tp_free (dest->tp, &dest->e);
      lockt_destroy (&dest->lt);
      return dest->e.cause_code;
    }

  dest->p = p;

  DBG_ASSERT (pgr_fixture, dest);

  return SUCCESS;
}

err_t
pgr_fixture_teardown (struct pgr_fixture *f)
{
  pgr_close (f->p, &f->e);

  tp_free (f->tp, &f->e);
  lockt_destroy (&f->lt);

  return f->e.cause_code;
}

err_t
build_fake_inner_node (page_h *dest, struct in_page_builder b, error *e)
{
  DBG_ASSERT (in_page_builder, &b);

  err_t_wrap (pgr_new (dest, b.pager, b.txn, PG_INNER_NODE, e), e);

  // Create or link
  if (b.prev)
    {
      err_t_wrap (pgr_maybe_make_writable (b.pager, b.txn, b.prev, e), e);
      dlgt_link (page_h_w (b.prev), page_h_w (dest));
    }

  if (b.next)
    {
      err_t_wrap (pgr_maybe_make_writable (b.pager, b.txn, b.next, e), e);
      dlgt_link (page_h_w (dest), page_h_w (b.next));
    }

  // Populate data
  struct in_pair data[IN_MAX_KEYS];

  // Copy explicit children
  if (b.children.nodes)
    {
      i_memcpy (data, b.children.nodes, b.children.len * sizeof (struct in_pair));
    }

  static pgno pg = 99999;

  // Fill rest with random
  if (b.children.len < b.dclen)
    {
      for (u32 i = b.children.len; i < b.dclen; ++i)
        {
          // Can't duplicate nodes - I'm assuming user supplied nodes will be < 99999
          data[i].pg = pg++;
          data[i].key = 1;
        }
    }

  in_set_data (page_h_w (dest), (struct in_data){ .nodes = data, .len = b.dclen });

  return 0;
}

err_t
build_fake_data_list (page_h *dest, struct dl_page_builder b, error *e)
{
  DBG_ASSERT (dl_page_builder, &b);

  err_t_wrap (pgr_new (dest, b.pager, b.txn, PG_DATA_LIST, e), e);

  // Create or link
  if (b.prev)
    {
      err_t_wrap (pgr_maybe_make_writable (b.pager, b.txn, b.prev, e), e);
      dlgt_link (page_h_w (b.prev), page_h_w (dest));
    }

  if (b.next)
    {
      err_t_wrap (pgr_maybe_make_writable (b.pager, b.txn, b.next, e), e);
      dlgt_link (page_h_w (dest), page_h_w (b.next));
    }

  // Populate data
  u8 data[DL_DATA_SIZE];

  // Copy explicit children
  if (b.data.data)
    {
      i_memcpy (data, b.data.data, b.data.blen);
    }

  // Fill rest with random
  if (b.data.blen < b.dclen)
    {
      rand_bytes (&data[b.data.blen], (b.dclen - b.data.blen));
    }

  dl_set_data (page_h_w (dest), (struct dl_data){ .data = data, .blen = b.dclen });

  return 0;
}

//////////////////////////////////
/// DECLARATIVE API

static err_t build_page_desc (
    struct page_desc *desc,
    struct pager *pager,
    struct txn *txn,
    error *e);

err_t
build_page_tree (struct page_tree_builder *builder, error *e)
{
  return build_page_desc (&builder->root, builder->pager, builder->txn, e);
}

static err_t
build_page_desc (struct page_desc *desc, struct pager *pager, struct txn *txn, error *e)
{
  switch (desc->type)
    {
    case PG_INNER_NODE:
      {
        // Build children nodes first
        page_h *prev = NULL;
        struct in_pair children[IN_MAX_KEYS];

        for (u32 i = 0; i < desc->inner.clen; i++)
          {
            page_h *cur = &desc->inner.children[i].out;
            ASSERT (cur->mode == PHM_NONE);

            err_t_wrap (build_page_desc (&desc->inner.children[i], pager, txn, e), e);

            if (prev)
              {
                dlgt_link (page_h_w (prev), page_h_w (cur));
              }

            prev = cur;

            children[i] = (struct in_pair){
              .pg = page_h_pgno (&desc->inner.children[i].out),
              .key = desc->inner.children[i].size,
            };
          }

        // Build the inner node
        struct in_page_builder builder = {
          .pager = pager,
          .txn = txn,
          .prev = NULL,
          .next = NULL, // Links one layer up
          .children = (struct in_data){
              .nodes = children,
              .len = desc->inner.clen,
          },
          .dclen = desc->inner.dclen
        };

        err_t_wrap (build_fake_inner_node (&desc->out, builder, e), e);
        desc->size = in_get_size (page_h_ro (&desc->out));

        return SUCCESS;
      }

    case PG_DATA_LIST:
      {
        // Build data list page
        struct dl_page_builder builder = {
          .pager = pager,
          .txn = txn,
          .prev = NULL,
          .next = NULL, // Links one layer up
          .data = desc->data_list,
          .dclen = desc->size,
        };

        return build_fake_data_list (&desc->out, builder, e);
      }

    default:
      {
        UNREACHABLE ();
      }
    }

  return 0;
}

static err_t
page_desc_release_all (struct page_desc *b, struct pager *p, error *e)
{
  pgr_release (p, &b->out, b->type, e);

  if (b->type == PG_INNER_NODE)
    {
      for (u32 i = 0; i < b->inner.clen; ++i)
        {
          page_desc_release_all (&b->inner.children[i], p, e);
        }
    }

  return e->cause_code;
}

err_t
page_tree_builder_release_all (struct page_tree_builder *b, error *e)
{
  return page_desc_release_all (&b->root, b->pager, e);
}

TEST (TT_UNIT, build_page_tree)
{
  struct pgr_fixture f;
  test_err_t_wrap (pgr_fixture_create (&f), &f.e);

  struct txn tx;
  test_err_t_wrap (pgr_begin_txn (&tx, f.p, &f.e), &f.e);

  struct page_tree_builder builder = {
    .root = {
        .type = PG_INNER_NODE,
        .out = page_h_create (),
        .inner = {
            .dclen = 3,
            .clen = 2,
            .children = (struct page_desc[]){
                {
                    .type = PG_INNER_NODE,
                    .out = page_h_create (),
                    .inner = {
                        .dclen = IN_MAX_KEYS,
                        .clen = 1,
                        .children = (struct page_desc[]){
                            {
                                .type = PG_DATA_LIST,
                                .out = page_h_create (),
                                .size = DL_DATA_SIZE,
                                .data_list = (struct dl_data){ .data = NULL, .blen = 0 },
                            },
                        },

                    },
                },

                {
                    .type = PG_DATA_LIST,
                    .out = page_h_create (),
                    .size = DL_DATA_SIZE,
                    .data_list = (struct dl_data){ .data = NULL, .blen = 0 },
                },
            },
        },
    },
    .pager = f.p,
    .txn = &tx,
  };

  test_err_t_wrap (build_page_tree (&builder, &f.e), &f.e);

  page_h *root = &builder.root.out;
  page_h *inl1 = &builder.root.inner.children[0].out;
  page_h *dll1 = &builder.root.inner.children[1].out;
  page_h *dll2 = &builder.root.inner.children[0].inner.children[0].out;

  test_err_t_wrap (pgr_release (f.p, root, PG_INNER_NODE, &f.e), &f.e);
  test_err_t_wrap (pgr_release (f.p, inl1, PG_INNER_NODE, &f.e), &f.e);
  test_err_t_wrap (pgr_release (f.p, dll1, PG_DATA_LIST, &f.e), &f.e);
  test_err_t_wrap (pgr_release (f.p, dll2, PG_DATA_LIST, &f.e), &f.e);

  test_err_t_wrap (pgr_commit (f.p, &tx, &f.e), &f.e);

  test_err_t_wrap (pgr_fixture_teardown (&f), &f.e);
}
#endif
