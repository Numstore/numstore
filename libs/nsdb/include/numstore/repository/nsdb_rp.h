#pragma once

#include "numstore/types/type_ref.h"
#include <numstore/datasource/rptc_ds.h>
#include <numstore/datasource/var_ds.h>

/**
 * Numstore repository.
 *
 * Source of truth for fetching
 * any type of information you
 * want from the database
 */
struct nsdb_rp
{
  struct rptc_ds r;
  struct var_ds v;
};

err_t nsdb_rp_init (struct nsdb_rp *dest, struct pager *p, struct lockt *lt, error *e);
void nsdb_rp_free (struct nsdb_rp *r);

////////////////////
/// CURSOR FETCHING
struct rptree_cursor *nsdb_rp_open_cursor (struct nsdb_rp *r, struct string vname, error *e);
err_t nsdb_rp_close_cursor (struct nsdb_rp *r, struct rptree_cursor *c, error *e);

////////////////////
/// VARIABLE FETCHING
const struct variable *nsdb_rp_get_variable (struct nsdb_rp *v, struct string vname, error *e);
err_t nsdb_rp_free_variable (struct nsdb_rp *v, const struct variable *vr, error *e);

////////////////////
/// VARIABLE MUTATION
err_t nsdb_rp_create (struct nsdb_rp *v, struct txn *tx, struct string vname, struct type *t, error *e);
err_t nsdb_rp_update (struct nsdb_rp *v, struct txn *tx, const struct variable *var, pgno newpg, b_size nbytes, error *e);
err_t nsdb_rp_delete (struct nsdb_rp *v, struct txn *tx, struct string vname, error *e);

////////////////////
/// RESULT SET CREATION
struct result_set *nsdb_rp_get_result_set (
    struct nsdb_rp *rp,
    struct type_ref *tr,            // struct { a a.b.c[0], b a.b, c a }
    struct chunk_alloc *persistent, // Where to allocate the query buffers
    struct user_stride stride,
    error *e);
