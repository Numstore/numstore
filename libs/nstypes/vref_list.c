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
