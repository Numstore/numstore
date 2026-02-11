#include <numstore/types/type_ref.h>

bool
type_ref_equal (const struct type_ref left, const struct type_ref right)
{
  if (left.type != right.type)
    {
      return false;
    }

  switch (left.type)
    {
    case TR_TAKE:
      {
        return string_equal (left.tk.vname, right.tk.vname)
               && type_accessor_equal (left.tk.ta, right.tk.ta);
      }

    case TR_STRUCT:
      {
        if (left.st.len != right.st.len)
          {
            return false;
          }

        for (u16 i = 0; i < left.st.len; i++)
          {
            if (!string_equal (left.st.keys[i], right.st.keys[i]))
              {
                return false;
              }

            if (!type_ref_equal (left.st.types[i], right.st.types[i]))
              {
                return false;
              }
          }

        return true;
      }

    default:
      return false;
    }
}
