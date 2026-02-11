#include <numstore/rs/trfmrs.h>

#include <numstore/core/chunk_alloc.h>
#include <numstore/core/error.h>
#include <numstore/core/llist.h>
#include <numstore/rs/result_set.h>
#include <numstore/types/byte_accessor.h>
#include <numstore/types/subtype.h>
#include <numstore/types/type_accessor.h>
#include <numstore/types/types.h>

err_t
trfmrs_execute (struct result_set *rs, error *e)
{
  // Execute all children
  // TODO - don't need to do this for threaded execute
  for (u32 i = 0; i < rs->t.rslen; ++i)
    {
      err_t_wrap (rs_execute (rs->t.inputs[i], e), e);
    }

  while (true)
    {
      for (u32 i = 0; i < rs->t.rslen; ++i)
        {
          // Block on downstream
          // all inputs should have at least 1 element
          struct cbuffer *input = &rs->t.inputs[i]->output;
          t_size isize = type_byte_size (rs->t.inputs[i]->out_type);
          if (cbuffer_len (input) < isize)
            {
              return SUCCESS;
            }
        }

      // Block on upstream
      // output must have space for one combined write
      if (cbuffer_avail (&rs->output) < type_byte_size (rs->out_type))
        {
          return SUCCESS;
        }

      // Execute Predicate
      if (rs->t.predicate (rs->t.inputs, rs->t.rslen, rs->t.pctx))
        {
          // Write 1 element to destination buffer
          struct cbuffer *output = &rs->output;
          for (u32 i = 0; i < rs->t.aclen; ++i)
            {
              u32 rsidx = rs->t.vnums[i];
              struct byte_accessor *acc = rs->t.accs[i];
              struct cbuffer *input = &rs->t.inputs[rsidx]->output;

              // Note this does not consume data from buffer
              ba_memcpy_to (output, input, acc);
            }
        }

      // Clear all buffers and continue
      for (u32 i = 0; i < rs->t.rslen; ++i)
        {
          // Consume 1 element from each
          struct cbuffer *input = &rs->t.inputs[i]->output;
          t_size isize = type_byte_size (rs->t.inputs[i]->out_type);
          cbuffer_fakeread (input, isize);
        }
    }
}

////////////////////////////////////////////////////
/// BUILDER

void
trfmrsb_create (struct trfmrsb *dest, struct chunk_alloc *temp, struct chunk_alloc *persistent)
{
  dest->temp = temp;
  dest->persistent = persistent;
  dest->rs_head = NULL;
  dest->ta_head = NULL;
  dest->stride_supplied = false;

  kvlb_create (&dest->stbuilder, temp, persistent);
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
      // Not found - Append a new result set to the list
      struct trfmrsb_input_llnode *input = chunk_malloc (b->temp, 1, sizeof *input, e);
      if (input == NULL)
        {
          return e->cause_code;
        }
      input->rs = rs;
      llnode_init (&input->link);
      list_append (&b->rs_head, &input->link);
      idx = list_length (b->rs_head) - 1;
    }

  // Append new ta to the end of the list
  struct trfmrsb_ta_llnode *tanode = chunk_malloc (b->temp, 1, sizeof *tanode, e);
  if (tanode == NULL)
    {
      return e->cause_code;
    }

  if (kvlb_accept_key (&b->stbuilder, name, e))
    {
      return e->cause_code;
    }
  if (kvlb_accept_type (&b->stbuilder, subtype, e))
    {
      return e->cause_code;
    }

  tanode->idx = idx;
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

  if (!inputs || !accs || !vnums || !rettype)
    {
      return NULL;
    }

  u32 i = 0;
  struct llnode *iter = src->rs_head;
  for (; iter != NULL; iter = iter->next)
    {
      struct trfmrsb_input_llnode *node = container_of (iter, struct trfmrsb_input_llnode, link);
      inputs[i] = node->rs;
      i++;
    }

  i = 0;
  iter = src->ta_head;
  for (; iter != NULL; iter = iter->next)
    {
      struct trfmrsb_ta_llnode *node = container_of (iter, struct trfmrsb_ta_llnode, link);
      accs[i] = node->bacc;
      vnums[i] = node->idx;

      i++;
    }

  struct kvt_list kvl;
  if (kvlb_build (&kvl, &src->stbuilder, e))
    {
      return NULL;
    }
  if (struct_t_create (&rettype->st, kvl, src->persistent, e))
    {
      return NULL;
    }

  t_size size = type_byte_size (rettype);
  struct result_set *ret = chunk_malloc (src->persistent, 1, sizeof *ret + size, e);
  if (ret == NULL)
    {
      return NULL;
    }

  ret->t.inputs = inputs;
  ret->t.rslen = rslen;

  ret->t.accs = accs;
  ret->t.vnums = vnums;
  ret->t.aclen = aclen;

  ret->type = RS_TRANSFORM;
  ret->out_type = rettype;
  ret->output = cbuffer_create (ret->data, size);

  return ret;
}
