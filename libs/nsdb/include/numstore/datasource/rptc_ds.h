#pragma once

#include "numstore/core/adptv_hash_table.h"
#include "numstore/rptree/rptree_cursor.h"
#include "numstore/types/variables.h"
#include <numstore/core/hash_table.h>
#include <numstore/core/slab_alloc.h>
#include <numstore/core/stride.h>
#include <numstore/types/types.h>

/**
 * Takes in root, stride, type and returns a rptree result set
 */
struct rptc_ds
{
  struct slab_alloc alloc;
  struct pager *p;
  struct lockt *lt;
};

err_t rptds_init (struct rptc_ds *r, struct pager *p, error *e);
void rptds_free (struct rptc_ds *r);

struct rptree_cursor *rptds_open (struct rptc_ds *r, const struct variable *v, error *e);
err_t rptds_close (struct rptc_ds *r, struct rptree_cursor *c, error *e);
