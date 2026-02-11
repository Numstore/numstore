#pragma once

#include "numstore/datasource/rptc_ds.h"
#include "numstore/datasource/var_ds.h"
#include "numstore/types/type_ref.h"

struct rs_ds
{
  struct var_ds vds;
  struct rptc_ds rptds;
  struct chunk_alloc *alloc;
};

// TODO
err_t rs_ds_init (struct rs_ds *dest, struct pager *p, struct lockt *lt, error *e);
struct result_set *rsds_get_rptrs (struct string *vname, error *e);
struct result_set *rsds_get_trsfmrs (struct type_ref *ref, error *e);
