#include "numstore/core/error.h"
#include <numstore/types/variables.h>

err_t
validate_vname (struct string vname, error *e)
{
  if (vname.len == 0)
    {
      return error_causef (
          e, ERR_INVALID_ARGUMENT,
          "Variable name: %.*s must have length > 0",
          vname.len, vname.data);
    }

  if (vname.len >= 4096)
    {
      return error_causef (
          e, ERR_INVALID_ARGUMENT,
          "Maximum variable name is 4096 chars");
    }

  for (u32 i = 0; i < vname.len; ++i)
    {
      char c = vname.data[i];
      if (!is_alpha_num_generous (c))
        {
          return error_causef (
              e, ERR_INVALID_ARGUMENT,
              "Invalid string variable name: %.*s expect alpha numeric characters",
              vname.len, vname.data);
        }
    }

  return SUCCESS;
}
