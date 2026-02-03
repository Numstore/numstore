#include "numstore/core/chunk_alloc.h"
#include "numstore/core/error.h"
#include "numstore/nsdb/ba_bank.h"
#include "numstore/types/type_accessor.h"
#include <numstore/nsdb/multi_ba_bank.h>

err_t
mba_bank_init (
    struct nsdb *n,
    struct multi_ba_bank *dest,
    struct type_accessor_list src,
    struct vref_list *vbank,
    struct multi_var_bank *mvbank,
    error *e)
{
  dest->tlen = src.len;
  dest->tas = chunk_malloc (&n->chunka, src.len, sizeof (struct ba_bank), e);
  if (dest->tas == NULL)
    {
      return e->cause_code;
    }

  for (u32 i = 0; i < dest->tlen; ++i)
    {
      err_t_panic (ba_bank_init (
                       &dest->tas[i],
                       src.vnames[i],
                       &src.items[i],
                       vbank,
                       mvbank,
                       e),
                   e);
    }

  return SUCCESS;
}

t_size
mba_bank_byte_size (struct multi_ba_bank *ba)
{
  t_size ret = 0;
  for (u32 i = 0; i < ba->tlen; ++i)
    {
      ret += ba_byte_size (&ba->tas[i].acc);
    }

  return ret;
}
