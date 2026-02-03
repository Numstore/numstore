#pragma once

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/slab_alloc.h>

struct nsdb
{
  struct pager *p;
  struct lockt *lt;
  struct slab_alloc slaba;
  struct chunk_alloc chunka;
};
