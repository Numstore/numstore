#pragma once

#include <numstore/datasource/rptrs_ds.h>
#include <numstore/datasource/var_ds.h>

struct rptrs_rp
{
  struct var_ds vds;
  struct rptrs_ds rds;
};

err_t rptrs_rp_init (struct rptrs_rp *dest, struct chunk_alloc *alloc, error *e);
err_t rptrs_rp_close (struct rptrs_rp *v, error *e);

struct rptrs *rptrs_rp_open_rs (struct rptrs_rp *v, struct string vname, error *e);
struct rptrs *rptrs_rp_get_rs (struct rptrs_rp *v, struct string vname, error *e);
err_t rptrs_rp_close_rs (struct rptrs_rp *v, struct rptrs *r, error *e);
