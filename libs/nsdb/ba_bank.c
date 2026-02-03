#include "numstore/types/vref_list.h"
#include <numstore/nsdb/ba_bank.h>

err_t
ba_bank_init (
    struct ba_bank *dest,
    struct string valias,
    struct type_accessor *src,
    struct vref_list *vbank,
    struct multi_var_bank *mvbank,
    error *e)
{
  i32 loc = vrefl_find_variable (vbank, valias);

  if (loc == -1)
    {
      return error_causef (
          e, ERR_INVALID_ARGUMENT,
          "Variable: %.*s not in variable ref list",
          valias.len, valias.data);
    }

  dest->bank_id = loc;

  return type_to_byte_accessor (&dest->acc, src, mvbank->vars[loc].type, e);
}
