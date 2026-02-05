#include "numstore/core/adptv_hash_table.h"
#include "numstore/core/chunk_alloc.h"
#include "numstore/core/clock_allocator.h"
#include "numstore/core/error.h"
#include <numstore/core/hash_table.h>
#include <numstore/core/hashing.h>
#include <numstore/core/slab_alloc.h>
#include <numstore/core/string.h>
#include <numstore/types/vbank.h>

struct vbank_entry
{
  struct var_with_alias ref;
  struct hnode node;
};

err_t
vbank_init (struct vbank *dest, struct chunk_alloc *alloc, error *e)
{
  dest->table = htable_create (512, e);
  if (dest->table == NULL)
    {
      return e->cause_code;
    }
  dest->alloc = alloc;
  return SUCCESS;
}

void
vbank_close (struct vbank *v)
{
  htable_free (v->table);
}

err_t
vbank_insert (
    struct vbank *v,
    struct string vname,
    struct string alias,
    struct type *type,
    error *e)
{
  struct vbank_entry *refe = chunk_malloc (v->alloc, 1, sizeof *refe, e);
  if (refe == NULL)
    {
      return e->cause_code;
    }

  u8 *type_data = chunk_malloc (v->alloc, 1, type_byte_size (type), e);
  if (type_data == NULL)
    {
      return e->cause_code;
    }

  refe->ref = (struct var_with_alias){
    .alias = alias,
    .vname = vname,
    .data = type_data,
    .vtype = type,
  };

  hnode_init (&refe->node, fnv1a_hash (alias));
  htable_insert (v->table, &refe->node);

  return SUCCESS;
}

static bool
var_with_alias_eq (const struct hnode *left, const struct hnode *right)
{
  struct vbank_entry *rentry = container_of (right, struct vbank_entry, node);
  struct vbank_entry *lentry = container_of (right, struct vbank_entry, node);

  return string_equal (rentry->ref.vname, lentry->ref.vname);
}

struct var_with_alias *
vbank_get (struct vbank *v, struct string alias)
{
  struct vbank_entry key = {
    .ref = {
        .alias = alias,
    },
  };
  hnode_init (&key.node, fnv1a_hash (alias));

  struct hnode **_node = htable_lookup (v->table, &key.node, var_with_alias_eq);
  ASSERT (_node);
  struct hnode *node = *_node;

  return &container_of (node, struct vbank_entry, node)->ref;
}
