#include "numstore/core/chunk_alloc.h"
#include "numstore/core/error.h"
#include "numstore/nsdb/var_bank.h"
#include "numstore/types/vref_list.h"
#include <numstore/nsdb/multi_var_bank.h>

err_t
mvar_bank_open_all (
    struct nsdb *n,
    struct multi_var_bank *dest,
    struct vref_list vars,
    struct user_stride stride,
    error *e)
{
  dest->vlen = vars.len;
  dest->vars = chunk_malloc (&n->chunka, vars.len, sizeof (struct var_bank), e);
  if (dest->vars == NULL)
    {
      return e->cause_code;
    }

  // Open all var_banks
  for (u32 i = 0; i < vars.len; ++i)
    {
      err_t_panic (var_bank_open (n, &dest->vars[i], vars.items[i].vname, stride, e), e);
    }

  return SUCCESS;
}

err_t
mvar_bank_execute (struct multi_var_bank *v, error *e)
{
  for (u32 i = 0; i < v->vlen; ++i)
    {
      err_t_panic (var_bank_execute (&v->vars[i], e), e);
    }

  return SUCCESS;
}

err_t
mvar_bank_close (struct nsdb *n, struct multi_var_bank *v, error *e)
{
  for (u32 i = 0; i < v->vlen; ++i)
    {
      err_t_panic (var_bank_close (n, &v->vars[i], e), e);
    }

  return SUCCESS;
}
