#include <numstore/core/assert.h>
#include <numstore/types/type_accessor.h>

void
tab_create (
    struct type_accessor_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent)
{
  *dest = (struct type_accessor_builder){
    .head = NULL,
    .tail = NULL,
    .temp = temp,
    .persistent = persistent,
  };
}

err_t
tab_accept_select (struct type_accessor_builder *builder, struct string key, error *e)
{
  /* Allocate new accessor */
  struct type_accessor *ta = chunk_malloc (builder->temp, 1, sizeof *ta, e);
  if (!ta)
    {
      return e->cause_code;
    }

  /* Copy key to persistent memory */
  key.data = chunk_alloc_move_mem (builder->persistent, key.data, key.len, e);
  if (!key.data)
    {
      return e->cause_code;
    }

  /* Initialize select accessor */
  ta->type = TA_SELECT;
  ta->select.key = key;
  ta->select.sub_ta = NULL;

  /* Link into chain */
  if (!builder->head)
    {
      builder->head = ta;
      builder->tail = ta;
    }
  else
    {
      builder->tail->select.sub_ta = ta;
      builder->tail = ta;
    }

  return SUCCESS;
}

err_t
tab_accept_range (
    struct type_accessor_builder *builder,
    t_size start,
    t_size stop,
    t_size step,
    error *e)
{
  /* Allocate new accessor */
  struct type_accessor *ta = chunk_malloc (builder->temp, 1, sizeof *ta, e);
  if (!ta)
    {
      return e->cause_code;
    }

  /* Initialize range accessor */
  ta->type = TA_RANGE;
  ta->range.start = start;
  ta->range.stop = stop;
  ta->range.step = step;
  ta->range.sub_ta = NULL;

  /* Link into chain */
  if (!builder->head)
    {
      builder->head = ta;
      builder->tail = ta;
    }
  else
    {
      /* Need to set sub_ta based on previous type */
      if (builder->tail->type == TA_SELECT)
        {
          builder->tail->select.sub_ta = ta;
        }
      else if (builder->tail->type == TA_RANGE)
        {
          builder->tail->range.sub_ta = ta;
        }
      builder->tail = ta;
    }

  return SUCCESS;
}

err_t
tab_build (struct type_accessor **dest, struct type_accessor_builder *builder, error *e)
{
  if (!builder->head)
    {
      return error_causef (e, ERR_INTERP, "Type accessor builder is empty");
    }

  /* Move the chain from temp to persistent memory */
  struct type_accessor *current = builder->head;
  struct type_accessor *prev_persistent = NULL;
  struct type_accessor *head_persistent = NULL;

  while (current)
    {
      /* Allocate in persistent memory */
      struct type_accessor *persistent_node = chunk_malloc (builder->persistent, 1, sizeof *persistent_node, e);
      if (!persistent_node)
        {
          return e->cause_code;
        }

      /* Copy the node */
      *persistent_node = *current;

      /* Link into persistent chain */
      if (!head_persistent)
        {
          head_persistent = persistent_node;
        }
      if (prev_persistent)
        {
          if (prev_persistent->type == TA_SELECT)
            {
              prev_persistent->select.sub_ta = persistent_node;
            }
          else if (prev_persistent->type == TA_RANGE)
            {
              prev_persistent->range.sub_ta = persistent_node;
            }
        }

      /* Advance */
      prev_persistent = persistent_node;
      if (current->type == TA_SELECT)
        {
          current = current->select.sub_ta;
        }
      else if (current->type == TA_RANGE)
        {
          current = current->range.sub_ta;
        }
      else
        {
          current = NULL;
        }
    }

  *dest = head_persistent;
  return SUCCESS;
}
