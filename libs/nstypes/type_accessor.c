#include "numstore/core/assert.h"
#include <numstore/types/type_accessor.h>

bool
type_accessor_equal (const struct type_accessor left, const struct type_accessor right)
{
  if (left.type != right.type)
    {
      return false;
    }

  switch (left.type)
    {
    case TA_TAKE:
      {
        return true;
      }
    case TA_SELECT:
      {
        if (!string_equal (left.select.key, right.select.key))
          {
            return false;
          }
        return type_accessor_equal (*left.select.sub_ta, *right.select.sub_ta);
      }
    case TA_RANGE:
      {
        if (!user_stride_equal (&left.range.stride, &right.range.stride))
          {
            return false;
          }
        return type_accessor_equal (*left.range.sub_ta, *right.range.sub_ta);
      }
    }

  return false;
}

err_t
ta_subtype (
    struct type *dest,
    struct type *reftype,
    struct type_accessor *ta,
    error *e)
{
  switch (ta->type)
    {
    case TA_TAKE:
      {
        *dest = *reftype;
        return SUCCESS;
      }
    case TA_SELECT:
      {
        switch (reftype->type)
          {
          case T_STRUCT:
            {
              for (u32 i = 0; i < reftype->st.len; ++i)
                {
                  if (string_equal (reftype->st.keys[i], ta->select.key))
                    {
                      return ta_subtype (dest, &reftype->st.types[i], ta->select.sub_ta, e);
                    }
                }
              return error_causef (
                  e, ERR_INVALID_ARGUMENT,
                  "Invalid struct key: %.*s",
                  ta->select.key.len, ta->select.key.data);
            }
          case T_UNION:
            {
              for (u32 i = 0; i < reftype->un.len; ++i)
                {
                  if (string_equal (reftype->un.keys[i], ta->select.key))
                    {
                      return ta_subtype (dest, &reftype->un.types[i], ta->select.sub_ta, e);
                    }
                }
              return error_causef (
                  e, ERR_INVALID_ARGUMENT,
                  "Invalid union key: %.*s",
                  ta->select.key.len, ta->select.key.data);
            }
          case T_PRIM:
          case T_SARRAY:
          case T_ENUM:
            {
              return error_causef (e, ERR_INVALID_ARGUMENT, "Cannot select a non selectable type");
            }
          }
        UNREACHABLE ();
      }
    case TA_RANGE:
      {
        switch (reftype->type)
          {
          case T_SARRAY:
            {
              return ta_subtype (dest, reftype->sa.t, ta->range.sub_ta, e);
            }
          case T_STRUCT:
          case T_UNION:
          case T_PRIM:
          case T_ENUM:
            {
              return error_causef (e, ERR_INVALID_ARGUMENT, "Cannot range on a non rangable type");
            }
          }
        UNREACHABLE ();
      }
    }

  UNREACHABLE ();
}
