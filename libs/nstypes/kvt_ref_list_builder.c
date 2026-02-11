#include <numstore/core/assert.h>
#include <numstore/intf/logging.h>
#include <numstore/types/type_ref.h>

DEFINE_DBG_ASSERT (
    struct kvt_ref_list_builder, kvt_ref_list_builder, s,
    {
      ASSERT (s);
      ASSERT (s->klen <= 10);
      ASSERT (s->tlen <= 10);
    })

void
kvrlb_create (struct kvt_ref_list_builder *dest, struct chunk_alloc *temp, struct chunk_alloc *persistent)
{
  *dest = (struct kvt_ref_list_builder){
    .head = NULL,
    .klen = 0,
    .tlen = 0,
    .temp = temp,
    .persistent = persistent,
  };
  DBG_ASSERT (kvt_ref_list_builder, dest);
}

static bool
kvrlb_has_key_been_used (const struct kvt_ref_list_builder *ub, struct string key)
{
  for (struct llnode *it = ub->head; it; it = it->next)
    {
      struct kv_ref_llnode *kn = container_of (it, struct kv_ref_llnode, link);
      if (string_equal (kn->key, key))
        {
          return true;
        }
    }
  return false;
}

err_t
kvrlb_accept_key (struct kvt_ref_list_builder *ub, struct string key, error *e)
{
  DBG_ASSERT (kvt_ref_list_builder, ub);

  /* Check for duplicate keys */
  if (kvrlb_has_key_been_used (ub, key))
    {
      return error_causef (
          e, ERR_INTERP,
          "Key: %.*s has already been used",
          key.len, key.data);
    }

  // Copy key data to persistent memory
  key.data = chunk_alloc_move_mem (ub->persistent, key.data, key.len, e);
  if (key.data == NULL)
    {
      return e->cause_code;
    }

  /* Find where to insert this new key in the linked list */
  struct llnode *slot = llnode_get_n (ub->head, ub->klen);
  struct kv_ref_llnode *node;
  if (slot)
    {
      node = container_of (slot, struct kv_ref_llnode, link);
    }
  else
    {
      /* Allocate new node onto temp */
      node = chunk_malloc (ub->temp, 1, sizeof *node, e);
      if (!node)
        {
          return e->cause_code;
        }
      llnode_init (&node->link);
      node->value = (struct type_ref){ 0 };

      /* Set the head if it doesn't exist */
      if (!ub->head)
        {
          ub->head = &node->link;
        }
      /* Otherwise, append to the list */
      else
        {
          list_append (&ub->head, &node->link);
        }
    }

  // Set the node key
  node->key = key;
  ub->klen++;

  return SUCCESS;
}

err_t
kvrlb_accept_type (struct kvt_ref_list_builder *ub, struct type_ref t, error *e)
{
  DBG_ASSERT (kvt_ref_list_builder, ub);

  struct llnode *slot = llnode_get_n (ub->head, ub->tlen);
  struct kv_ref_llnode *node;
  if (slot)
    {
      node = container_of (slot, struct kv_ref_llnode, link);
    }
  else
    {
      node = chunk_malloc (ub->temp, 1, sizeof *node, e);
      if (!node)
        {
          return e->cause_code;
        }
      llnode_init (&node->link);
      node->key = (struct string){ 0 };
      if (!ub->head)
        {
          ub->head = &node->link;
        }
      else
        {
          list_append (&ub->head, &node->link);
        }
    }

  node->value = t;
  ub->tlen++;
  return SUCCESS;
}

err_t
kvrlb_build (struct kvt_ref_list *dest, struct kvt_ref_list_builder *ub, error *e)
{
  ASSERT (dest);

  if (ub->klen == 0)
    {
      return error_causef (
          e, ERR_INTERP,
          "Expecting at least one key");
    }
  if (ub->klen != ub->tlen)
    {
      return error_causef (
          e, ERR_INTERP,
          "Must have same number of keys and values");
    }

  struct string *keys = chunk_malloc (ub->persistent, ub->klen, sizeof *keys, e);
  if (!keys)
    {
      return e->cause_code;
    }

  struct type_ref *types = chunk_malloc (ub->persistent, ub->tlen, sizeof *types, e);
  if (!types)
    {
      return e->cause_code;
    }

  size_t i = 0;
  for (struct llnode *it = ub->head; it; it = it->next)
    {
      struct kv_ref_llnode *kn = container_of (it, struct kv_ref_llnode, link);
      keys[i] = kn->key;
      types[i] = kn->value;
      i++;
    }

  dest->keys = keys;
  dest->types = types;
  dest->len = ub->klen;

  return SUCCESS;
}
