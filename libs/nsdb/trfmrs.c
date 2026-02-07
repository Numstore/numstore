#include "numstore/core/chunk_alloc.h"
#include "numstore/core/error.h"
#include "numstore/core/llist.h"
#include "numstore/types/byte_accessor.h"
#include "numstore/types/subtype.h"
#include "numstore/types/type_accessor.h"
#include "numstore/types/types.h"
#include <numstore/rs/result_set.h>
#include <numstore/rs/trfmrs.h>

void
trfmrsb_create (struct trfmrsb *dest, struct chunk_alloc *temp, struct chunk_alloc *persistent)
{
  dest->temp = temp;
  dest->persistent = persistent;
  dest->rs_head = NULL;
  dest->ta_head = NULL;
  dest->stride_supplied = false;
}

static bool
rs_source_eq (const struct llnode *left, const struct llnode *right)
{
  struct trfmrsb_input_llnode *lnode = container_of (left, struct trfmrsb_input_llnode, link);
  struct trfmrsb_input_llnode *rnode = container_of (right, struct trfmrsb_input_llnode, link);
  return lnode->rs == rnode->rs;
}

err_t
trfmrsb_append_select (
    struct trfmrsb *b,
    struct result_set *rs,
    struct type_accessor *ta,
    struct string name,
    error *e)
{
  // Convert to a byte accessor
  struct byte_accessor *ba = chunk_malloc (b->persistent, 1, sizeof *ba, e);
  if (ba == NULL || type_to_byte_accessor (ba, ta, rs->out_type, b->persistent, e))
    {
      return e->cause_code;
    }

  // Extract the subtype
  struct type subtype;
  if (ta_subtype (&subtype, rs->out_type, ta, e))
    {
      return e->cause_code;
    }

  // Search for this rs if it already exists
  struct trfmrsb_input_llnode node = {
    .rs = rs,
  };
  llnode_init (&node.link);
  u32 idx;

  if (list_find (&idx, b->rs_head, &node.link, rs_source_eq) == NULL)
    {
      // Note found - Append a new result set to the list
      struct trfmrsb_input_llnode *input = chunk_malloc (b->temp, 1, sizeof *input, e);
      input->rs = rs;
      llnode_init (&input->link);
      return SUCCESS;
    }

  // Append new ta to the end of the list
  struct trfmrsb_ta_llnode *tanode = chunk_malloc (b->temp, 1, sizeof *ta, e);
  tanode->idx = idx; // 1 + len if not found
  tanode->bacc = ba;
  tanode->name = name;
  tanode->t = subtype;
  llnode_init (&tanode->link);
  list_append (&b->ta_head, &tanode->link);

  return SUCCESS;
}

err_t
trfmrsb_add_slice (struct trfmrsb *b, struct user_stride stride, error *e)
{
  if (b->stride_supplied)
    {
      return error_causef (e, ERR_INVALID_ARGUMENT, "Stride already supplied to transform result set builder");
    }
  b->stride_supplied = true;
  b->stride = stride;
  return SUCCESS;
}

struct result_set *
trfmrsb_build (struct trfmrsb *src, error *e)
{
  u32 rslen = list_length (src->rs_head);
  u32 aclen = list_length (src->ta_head);

  if (rslen == 0 || aclen == 0)
    {
      error_causef (
          e, ERR_INVALID_ARGUMENT,
          "Transform result set builder - must supply at least one source");
      return NULL;
    }

  if (!src->stride_supplied)
    {
      src->stride = (struct user_stride){
        .present = ~0,
        .start = 0,
        .stop = -1,
        .step = 1,
      };
      src->stride_supplied = true;
    }

  struct result_set **inputs = chunk_malloc (src->persistent, rslen, sizeof (struct result_set *), e);
  struct byte_accessor **accs = chunk_malloc (src->persistent, aclen, sizeof (struct byte_accessor *), e);
  u32 *vnums = chunk_malloc (src->persistent, aclen, sizeof (u32), e);
  struct type *rettype = chunk_malloc (src->temp, 1, sizeof *rettype, e);
  struct string *struct_keys = chunk_malloc (src->persistent, aclen, sizeof (struct string), e);
  struct type *struct_types = chunk_malloc (src->persistent, aclen, sizeof (struct type), e);

  if (!inputs || !accs || !vnums || !rettype || !struct_keys || !struct_types)
    {
      return NULL;
    }

  u32 i = 0;
  struct llnode *iter = src->rs_head;
  for (; iter != NULL; iter = iter->next)
    {
      struct trfmrsb_input_llnode *node = container_of (iter, struct trfmrsb_input_llnode, link);
      inputs[i] = node->rs;
    }

  iter = src->ta_head;
  for (; iter != NULL; iter = iter->next)
    {
      struct trfmrsb_ta_llnode *node = container_of (iter, struct trfmrsb_ta_llnode, link);
      accs[i] = node->bacc;
      vnums[i] = node->idx;

      struct_keys[i] = node->name;
      struct_types[i] = node->t;
    }

  rettype->type = T_STRUCT;
  rettype->st = (struct struct_t){
    .len = aclen,
    .types = struct_types,
    .keys = struct_keys,
  };

  t_size size = type_byte_size (rettype);
  struct result_set *ret = chunk_malloc (src->persistent, 1, sizeof *ret + size, e);

  ret->t.inputs = inputs;
  ret->t.rslen = rslen;

  ret->t.accs = accs;
  ret->t.vnums = vnums;
  ret->t.aclen = aclen;

  ret->out_type = rettype;
  ret->output = cbuffer_create (ret->data, size);

  return ret;
}
