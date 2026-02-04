#include <numstore/types/vref.h>

i32
vrefl_find_variable (struct vref_list *list, struct string vname)
{
  for (u32 i = 0; i < list->len; ++i)
    {
      if (string_equal (list->items[i].alias, vname))
        {
          return i;
        }
    }

  return -1;
}

bool
vref_equal (const struct vref *left, const struct vref *right)
{
  return string_equal (left->vname, right->vname) && string_equal (left->alias, right->alias);
}

bool
vref_list_equal (const struct vref_list *left, const struct vref_list *right)
{
  if (left->len != right->len)
    {
      return false;
    }

  for (u32 i = 0; i < left->len; ++i)
    {
      if (!vref_equal (&left->items[i], &right->items[i]))
        {
          return false;
        }
    }

  return true;
}
