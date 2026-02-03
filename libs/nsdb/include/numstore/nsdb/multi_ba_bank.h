#pragma once

#include <numstore/nsdb/ba_bank.h>
#include <numstore/types/type_accessor_list.h>

struct multi_ba_bank
{
  struct ba_bank *tas;
  u32 tlen;
};

err_t
mba_bank_init (
    struct nsdb *n,
    struct multi_ba_bank *dest,
    struct type_accessor_list src,
    struct vref_list *vbank,
    struct multi_var_bank *mvbank,
    error *e);

t_size mba_bank_byte_size (struct multi_ba_bank *ba);
