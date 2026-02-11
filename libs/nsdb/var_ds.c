#include <numstore/core/adptv_hash_table.h>
#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/hash_table.h>
#include <numstore/core/hashing.h>
#include <numstore/core/slab_alloc.h>
#include <numstore/core/string.h>
#include <numstore/datasource/var_ds.h>
#include <numstore/var/attr.h>
#include <numstore/var/var_cursor.h>

struct var_entry
{
  struct variable var;
  struct chunk_alloc alloc;
  struct hnode node;
};

err_t
vars_init (struct var_ds *dest, struct pager *p, error *e)
{
  struct adptv_htable_settings settings = {
    .min_size = 10,
    .max_size = 50000,
    .rehashing_work = 2,
    .min_load_factor = 0.25,
    .max_load_factor = 1.5,
  };

  err_t_wrap (varc_initialize (&dest->cursor, p, e), e);
  err_t_wrap (adptv_htable_init (&dest->vtable, settings, e), e);
  slab_alloc_init (&dest->alloc, sizeof (struct var_entry), 512);

  return SUCCESS;
}

void
vars_close (struct var_ds *v)
{
  adptv_htable_free (&v->vtable);
  slab_alloc_destroy (&v->alloc);
  return;
}

static bool
vequal (const struct hnode *left, const struct hnode *right)
{
  struct var_entry *lentry = container_of (left, struct var_entry, node);
  struct var_entry *rentry = container_of (left, struct var_entry, node);

  return string_equal (lentry->var.vname, rentry->var.vname);
}

static void
var_ds_freevar (struct var_ds *v, struct var_entry *entry)
{
  chunk_alloc_free_all (&entry->alloc);
  slab_alloc_free (&v->alloc, entry);
}

static struct var_entry *
var_ds_newvar (struct var_ds *v, struct string vname, error *e)
{
  // Create a new variable
  struct var_entry *ret = slab_alloc_alloc (&v->alloc, e);
  if (ret == NULL)
    {
      return NULL;
    }
  chunk_alloc_create_default (&ret->alloc);

  // Move data over to chunk alloc
  vname.data = chunk_alloc_move_mem (&ret->alloc, vname.data, vname.len, e);
  if (vname.data == NULL)
    {
      var_ds_freevar (v, ret);
      return NULL;
    }

  ret->var = (struct variable){
    .vname = vname,
    // Other fields left blank
  };

  return ret;
}

const struct variable *
vars_get (struct var_ds *v, struct string vname, error *e)
{
  struct var_entry key = {
    .var = {
        .vname = vname,
    },
  };
  hnode_init (&key.node, fnv1a_hash (vname));

  struct hnode *node = adptv_htable_lookup (&v->vtable, &key.node, vequal);

  // Doesn't exist in the cache
  if (node == NULL)
    {
      // Allocate new variable
      struct var_entry *ret = var_ds_newvar (v, vname, e);
      if (ret == NULL)
        {
          return NULL;
        }

      // Fetch variable from var cursor (source of truth)
      struct var_get_params params = {
        .vname = ret->var.vname,
      };
      if (vpc_get (&v->cursor, &ret->alloc, &params, e) < 0)
        {
          var_ds_freevar (v, ret);
          return NULL;
        }

      ret->var.nbytes = params.nbytes;
      ret->var.dtype = params.t;
      ret->var.rpt_root = params.rpt_root;
      ret->var.var_root = params.var_root;

      // Insert node into the cache
      hnode_init (&ret->node, fnv1a_hash (vname));
      if (adptv_htable_insert (&v->vtable, &ret->node, e))
        {
          var_ds_freevar (v, ret);
          return NULL;
        }

      return &ret->var;
    }
  else
    {
      return &container_of (node, struct var_entry, node)->var;
    }
}

err_t
vars_free (struct var_ds *v, const struct variable *var, error *e)
{
  // Delete it from the hash table
  struct var_entry *entry = container_of (var, struct var_entry, var);
  err_t_wrap (adptv_htable_delete (NULL, &v->vtable, &entry->node, vequal, e), e);

  // Free it's resources
  var_ds_freevar (v, entry);

  return SUCCESS;
}

err_t
vars_create (struct var_ds *v, struct txn *tx, struct string vname, struct type *t, error *e)
{
  struct var_create_params params = {
    .vname = vname,
    .t = *t,
  };

  varc_enter_transaction (&v->cursor, tx);
  if (vpc_new (&v->cursor, params, e))
    {
      varc_leave_transaction (&v->cursor);
      return e->cause_code;
    }

  varc_leave_transaction (&v->cursor);
  return SUCCESS;
}

err_t
vars_update (struct var_ds *v, struct txn *tx, const struct variable *var, pgno newpg, b_size nbytes, error *e)
{
  struct var_update_by_id_params params = {
    .id = var->var_root,
    .root = newpg,
    .nbytes = nbytes,
  };

  varc_enter_transaction (&v->cursor, tx);
  if (vpc_update_by_id (&v->cursor, &params, e))
    {
      varc_leave_transaction (&v->cursor);
      return e->cause_code;
    }

  varc_leave_transaction (&v->cursor);
  return SUCCESS;
}

err_t
vars_delete (struct var_ds *v, struct txn *tx, struct string vname, error *e)
{
  varc_enter_transaction (&v->cursor, tx);
  if (vpc_delete (&v->cursor, vname, e))
    {
      varc_leave_transaction (&v->cursor);
      return e->cause_code;
    }

  varc_leave_transaction (&v->cursor);
  return SUCCESS;
}
