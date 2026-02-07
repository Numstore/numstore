#pragma once

#include <numstore/core/hash_table.h>
#include <numstore/types/variables.h>
#include <numstore/var/var_cursor.h>

struct var_ds
{
  struct var_cursor cursor;   // Source of truth
  struct adptv_htable vtable; // For fetching unknown variables
  struct slab_alloc alloc;    // Allocator for variables
};

err_t vars_init (struct var_ds *dest, struct pager *p, error *e);
void vars_close (struct var_ds *v);

const struct variable *vars_get (struct var_ds *v, struct string vname, error *e);
err_t vars_free (struct var_ds *v, const struct variable *vr, error *e);

err_t vars_create (struct var_ds *v, struct txn *tx, struct string vname, struct type *t, error *e);
err_t vars_update (struct var_ds *v, struct txn *tx, const struct variable *var, pgno newpg, b_size nbytes, error *e);
err_t vars_delete (struct var_ds *v, struct txn *tx, struct string vname, error *e);
