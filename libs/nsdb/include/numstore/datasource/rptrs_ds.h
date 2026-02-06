#pragma once

#include <numstore/core/hash_table.h>
#include <numstore/core/stride.h>
#include <numstore/rs/rptrs.h>
#include <numstore/types/types.h>

/**
 * Takes in root, stride, type and returns a rptree result set
 */
struct rptrs_ds
{
  int temp;
};

err_t rptds_init (struct rptrs_ds *r, error *e);
err_t rptds_close (struct rptrs_ds *r, error *e);
struct result_set *rptds_open (struct rptrs_ds *r, pgno root, struct user_stride *stride, struct type *t, error *e);
err_t rptrs_close (struct rptrs_ds *r, struct rptrs *ds, error *e);
