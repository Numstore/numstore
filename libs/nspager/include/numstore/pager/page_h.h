#pragma once

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
 *   TODO: Add description for page_h.h
 */

#include <numstore/core/latch.h>
#include <numstore/pager/page.h>
#include <numstore/pager/page_delegate.h>

struct page_frame
{
  page page;
  u32 pin;
  u32 flags;
  i32 wsibling;
  struct latch latch;
};

typedef struct
{
  enum
  {
    PHM_NONE,
    PHM_S,
    PHM_X,
  } mode;

  // Read context stuff
  struct
  {
    struct page_frame *pgr;
  };

  // Write context stuff
  struct
  {
    struct page_frame *pgw;
    struct txn *tx;
  };
} page_h;

DEFINE_DBG_ASSERT (page_h, page_h, h, {
  ASSERT (h);

  switch (h->mode)
    {
    case PHM_S:
      {
        ASSERT (h->pgr);
        ASSERT (h->pgw == NULL);
        ASSERT (h->pgr->wsibling == -1);
        break;
      }
    case PHM_X:
      {
        ASSERT (h->pgr);
        ASSERT (h->pgw);
        ASSERT (h->pgr->wsibling >= 0);
        break;
      }
    case PHM_NONE:
      {
        ASSERT (h->pgr == NULL);
        ASSERT (h->pgw == NULL);
        break;
      }
    }
})

#define page_h_create() \
  (page_h)              \
  {                     \
    .mode = PHM_NONE,   \
    .pgr = NULL,        \
    .pgw = NULL,        \
  }

HEADER_FUNC void
page_h_xfer_ownership_ptr (page_h *dest, page_h *src)
{
  DBG_ASSERT (page_h, dest);
  DBG_ASSERT (page_h, src);
  if (dest->mode != PHM_NONE)
    {
      ASSERT (dest->mode == PHM_NONE);
      UNREACHABLE ();
    }
  *dest = *src;
  src->mode = PHM_NONE;
  src->pgr = NULL;
  src->pgw = NULL;
}

HEADER_FUNC page_h
page_h_xfer_ownership (page_h *h)
{
  DBG_ASSERT (page_h, h);
  page_h ret = *h;
  h->mode = PHM_NONE;
  h->pgr = NULL;
  h->pgw = NULL;
  return ret;
}

HEADER_FUNC const page *
page_h_r (const page_h *h)
{
  DBG_ASSERT (page_h, h);
  if (h->mode != PHM_S)
    {
      ASSERT (h->mode == PHM_S);
      UNREACHABLE ();
    }
  return &h->pgr->page;
}

HEADER_FUNC page *
page_h_w (const page_h *h)
{
  DBG_ASSERT (page_h, h);
  if (h->mode != PHM_X)
    {
      ASSERT (h->mode == PHM_X);
      UNREACHABLE ();
    }
  return &h->pgw->page;
}

HEADER_FUNC page *
page_h_w_or_null (const page_h *h)
{
  DBG_ASSERT (page_h, h);
  if (h->mode == PHM_NONE)
    {
      return NULL;
    }
  if (h->mode != PHM_X)
    {
      ASSERT (h->mode == PHM_X);
      UNREACHABLE ();
    }
  return &h->pgw->page;
}

HEADER_FUNC const page *
page_h_ro (const page_h *h)
{
  DBG_ASSERT (page_h, h);
  if (h->mode == PHM_X)
    {
      return &h->pgw->page;
    }
  else if (h->mode == PHM_S)
    {
      return &h->pgr->page;
    }

  ASSERT (h->mode != PHM_NONE);
  UNREACHABLE ();
}

HEADER_FUNC const page *
page_h_ro_or_null (const page_h *h)
{
  DBG_ASSERT (page_h, h);
  if (h->mode == PHM_NONE)
    {
      return NULL;
    }
  return page_h_ro (h);
}

HEADER_FUNC pgno
page_h_pgno (const page_h *h)
{
  DBG_ASSERT (page_h, h);
  const page *p = NULL;
  if (h->mode == PHM_X)
    {
      p = &h->pgw->page;
    }
  else if (h->mode == PHM_S)
    {
      p = &h->pgr->page;
    }
  else
    {
      ASSERT (h->mode != PHM_NONE);
      UNREACHABLE ();
    }
  return p->pg;
}

HEADER_FUNC pgno
page_h_pgno_or_null (const page_h *h)
{
  if (h->mode == PHM_NONE)
    {
      return PGNO_NULL;
    }
  return page_h_pgno (h);
}

HEADER_FUNC enum page_type
page_h_type (const page_h *h)
{
  DBG_ASSERT (page_h, h);
  const page *p = NULL;
  if (h->mode == PHM_X)
    {
      p = &h->pgw->page;
    }
  else if (h->mode == PHM_S)
    {
      p = &h->pgr->page;
    }
  else
    {
      ASSERT (h->mode != PHM_NONE);
      UNREACHABLE ();
    }
  return page_get_type (p);
}

HEADER_FUNC struct in_pair
in_pair_from_pgh (page_h *pg)
{
  if (pg->mode == PHM_NONE)
    {
      return in_pair_empty;
    }
  return (struct in_pair){
    .pg = page_h_pgno (pg),
    .key = dlgt_get_size (page_h_ro (pg)),
  };
}
