#include <numstore/types/subtype.h>

#include <numstore/core/assert.h>
#include <numstore/test/testing.h>

bool
subtype_equal (const struct subtype *left, const struct subtype *right)
{
  return string_equal (left->vname, right->vname) && type_accessor_equal (&left->ta, &right->ta);
}

bool
subtype_list_equal (const struct subtype_list *left, const struct subtype_list *right)
{
  if (left->len != right->len)
    {
      return false;
    }

  for (u32 i = 0; i < left->len; ++i)
    {
      if (!subtype_equal (&left->items[i], &right->items[i]))
        {
          return false;
        }
    }

  return true;
}

DEFINE_DBG_ASSERT (
    struct subtype_list_builder, subtype_list_builder, s,
    {
      ASSERT (s);
    })

err_t
subtype_create (struct subtype *dest, struct string vname, struct type_accessor ta, error *e)
{
  *dest = (struct subtype){
    .vname = vname,
    .ta = ta,
  };

  // TODO - validate

  return SUCCESS;
}

void
stalb_create (
    struct subtype_list_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent)
{
  *dest = (struct subtype_list_builder){
    .head = NULL,
    .count = 0,
    .temp = temp,
    .persistent = persistent,
  };

  DBG_ASSERT (subtype_list_builder, dest);
}

err_t
stalb_accept (
    struct subtype_list_builder *builder,
    struct subtype acc,
    error *e)
{
  DBG_ASSERT (subtype_list_builder, builder);

  /* Allocate new node */
  struct ta_llnode *node = chunk_malloc (builder->temp, 1, sizeof *node, e);
  if (!node)
    {
      return e->cause_code;
    }

  llnode_init (&node->link);
  node->acc = acc;

  /* Add to list */
  if (!builder->head)
    {
      builder->head = &node->link;
    }
  else
    {
      list_append (&builder->head, &node->link);
    }
  builder->count++;

  return SUCCESS;
}

err_t
stalb_build (
    struct subtype_list *dest,
    struct subtype_list_builder *builder,
    error *e)
{
  DBG_ASSERT (subtype_list_builder, builder);
  ASSERT (dest);

  /* Empty list is valid - just return empty */
  if (builder->count == 0)
    {
      dest->items = NULL;
      dest->len = 0;
      return SUCCESS;
    }

  /* Allocate array in persistent memory */
  struct subtype *items = chunk_malloc (builder->persistent, builder->count, sizeof *items, e);
  if (!items)
    {
      return e->cause_code;
    }

  /* Copy items from linked list */
  u32 i = 0;
  for (struct llnode *it = builder->head; it; it = it->next)
    {
      struct ta_llnode *node = container_of (it, struct ta_llnode, link);
      items[i++] = node->acc;
    }

  dest->items = items;
  dest->len = builder->count;

  return SUCCESS;
}
