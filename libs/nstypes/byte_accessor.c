#include <numstore/core/assert.h>
#include <numstore/core/cbuffer.h>
#include <numstore/core/error.h>
#include <numstore/core/string.h>
#include <numstore/intf/os/memory.h>
#include <numstore/test/testing.h>
#include <numstore/types/byte_accessor.h>
#include <numstore/types/types.h>

static inline err_t
struct_t_select_ttoba (struct select_ba *dest, struct select_ta *src, struct struct_t *st, error *e)
{
  t_size bofst;
  struct type *subtype = struct_t_resolve_key (&bofst, st, src->key, e);
  if (subtype == NULL)
    {
      return e->cause_code;
    }

  struct byte_accessor *sub_ba = i_malloc (1, sizeof *sub_ba, e);
  if (sub_ba == NULL)
    {
      return e->cause_code;
    }

  err_t_wrap (type_to_byte_accessor (sub_ba, src->sub_ta, subtype, e), e);

  *dest = (struct select_ba){
    .bofst = bofst,
    .sub_ba = sub_ba,
  };

  return SUCCESS;
}

static inline err_t
union_t_select_ttoba (struct select_ba *dest, struct select_ta *src, struct union_t *un, error *e)
{
  struct type *subtype = union_t_resolve_key (un, src->key, e);
  if (subtype == NULL)
    {
      return e->cause_code;
    }

  struct byte_accessor *sub_ba = i_malloc (1, sizeof *sub_ba, e);
  if (sub_ba == NULL)
    {
      return e->cause_code;
    }

  err_t_wrap (type_to_byte_accessor (sub_ba, src->sub_ta, subtype, e), e);

  *dest = (struct select_ba){
    .bofst = 0,
    .sub_ba = sub_ba,
  };

  return SUCCESS;
}

static inline err_t
sarray_t_to_range_ttoba (struct range_ba *dest, struct range_ta *src, struct sarray_t *sa, error *e)
{
  t_size size = type_byte_size (sa->t);
  struct byte_accessor *sub_ba = i_malloc (1, sizeof *sub_ba, e);
  if (sub_ba == NULL)
    {
      return e->cause_code;
    }

  err_t_wrap (type_to_byte_accessor (sub_ba, src->sub_ta, sa->t, e), e);

  *dest = (struct range_ba){
    .bofst = size * src->start,
    .stride = size * src->step,
    .nelems = size * (src->stop - src->start),
    .sub_ba = sub_ba,
  };

  return SUCCESS;
}

err_t
type_to_byte_accessor (struct byte_accessor *dest, struct type_accessor *src, struct type *reftype, error *e)
{
  dest->type = src->type;
  dest->size = type_byte_size (reftype);

  switch (src->type)
    {
    case TA_TAKE:
      {
        return SUCCESS;
      }
    case TA_SELECT:
      {
        switch (reftype->type)
          {
          case T_STRUCT:
            {
              return struct_t_select_ttoba (&dest->select, &src->select, &reftype->st, e);
            }
          case T_UNION:
            {
              return union_t_select_ttoba (&dest->select, &src->select, &reftype->un, e);
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
              return sarray_t_to_range_ttoba (&dest->range, &src->range, &reftype->sa, e);
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

void
ta_memcpy_from_once (struct cbuffer *dest, struct cbuffer *src, struct byte_accessor *acc)
{
  ASSERT (cbuffer_avail (dest) >= ba_byte_size (acc));
  ASSERT (cbuffer_len (src) >= ba_byte_size (acc));

  switch (acc->type)
    {
    case TA_TAKE:
      {
        // Copy entire type here
        cbuffer_cbuffer_move (dest, 1, acc->size, src);
        return;
      }
    case TA_SELECT:
      {
        // Skip offset bytes, then recursively copy the selected field
        if (acc->select.bofst > 0)
          {
            cbuffer_read (NULL, 1, acc->select.bofst, src);
          }
        ta_memcpy_from_once (dest, src, acc->select.sub_ba);
        return;
      }
    case TA_RANGE:
      {
        t_size elem_size = ba_byte_size (acc->range.sub_ba);

        // Skip to start position
        if (acc->range.bofst > 0)
          {
            cbuffer_read (NULL, acc->range.bofst, elem_size, src);
          }

        t_size copied_count = 0;
        t_size pos = acc->range.bofst;

        while (pos < acc->range.nelems)
          {
            // Copy one element
            ta_memcpy_from_once (dest, src, acc->range.sub_ba);
            copied_count++;

            // Skip (stride - 1) elements to get to next one
            if (acc->range.stride > 1)
              {
                cbuffer_read (NULL, acc->range.stride - 1, elem_size, src);
              }

            // Advance position
            pos += acc->range.stride;
          }

        return;
      }
    }
  UNREACHABLE ();
}

t_size
ba_byte_size (struct byte_accessor *acc)
{
  switch (acc->type)
    {
    case TA_TAKE:
      {
        return acc->size;
      }
    case TA_SELECT:
      {
        return ba_byte_size (acc->select.sub_ba);
      }
    case TA_RANGE:
      {
        t_size elem_size = ba_byte_size (acc->range.sub_ba);
        t_size count = (acc->range.nelems - acc->range.bofst + acc->range.stride - 1) / acc->range.stride;
        return count * elem_size;
      }
    }
  UNREACHABLE ();
}

void
ta_memcpy_from (struct cbuffer *dest, struct cbuffer *src, struct byte_accessor *acc, u32 acclen)
{
  for (u32 i = 0; i < acclen; i++)
    {
      cbuffer_mark (src);
      ta_memcpy_from_once (dest, src, &acc[i]);
      cbuffer_reset (src);
    }
}

#ifndef NTEST
TEST (TT_UNIT, ta_memcpy_from_basic)
{
  // struct { a int, b struct { b char, c [5]u16 } }
  u8 src_buf[64];
  u8 dest_buf[64];
  struct cbuffer src = cbuffer_create (src_buf, 64);
  struct cbuffer dest = cbuffer_create (dest_buf, 64);

  // Write test data: a=0x12345678, b=0xAB, c={1,2,3,4,5}
  u8 test_data[] = {
    78, 56, 34, 12, // int a (little-endian)
    0xAB,           // char b
    1, 0,           // u16 c[0] = 1
    2, 0,           // u16 c[1] = 2
    3, 0,           // u16 c[2] = 3
    4, 0,           // u16 c[3] = 4
    5, 0,           // u16 c[4] = 5
  };
  cbuffer_write (test_data, 1, sizeof (test_data), &src);

  TEST_CASE ("[.a]")
  {
    cbuffer_discard_all (&dest);

    struct byte_accessor acc[] = {
      // SELECT(0, TAKE(4))
      {
          .type = TA_SELECT,
          .select = {
              .bofst = 0,
              .sub_ba = &(struct byte_accessor){
                  .type = TA_TAKE,
                  .size = 4,
              },
          },
      },
    };

    ta_memcpy_from (&dest, &src, acc, 1);

    u8 out[4];
    cbuffer_read (out, 1, 4, &dest);
    test_assert_int_equal (out[0], 78);
    test_assert_int_equal (out[1], 56);
    test_assert_int_equal (out[2], 34);
    test_assert_int_equal (out[3], 12);
  }

  TEST_CASE ("[.b]")
  {
    struct byte_accessor acc[] = {
      // SELECT(4, TAKE(1))
      {
          .type = TA_SELECT,
          .select = {
              .bofst = 4,
              .sub_ba = &(struct byte_accessor){
                  .type = TA_TAKE,
                  .size = 1,
              },
          },
      },
    };

    cbuffer_discard_all (&dest);
    ta_memcpy_from (&dest, &src, acc, 1);

    u8 out[4];
    cbuffer_read (out, 1, 1, &dest);
    test_assert_int_equal (out[0], 0xAB);
  }

  TEST_CASE ("[.b, .a]")
  {
    cbuffer_discard_all (&dest);

    struct byte_accessor acc[] = {
      // SELECT(4, TAKE(1))
      {
          .type = TA_SELECT,
          .select = {
              .bofst = 4,
              .sub_ba = &(struct byte_accessor){
                  .type = TA_TAKE,
                  .size = 1,
              },
          },
      },
      // SELECT(0, TAKE(4))
      {
          .type = TA_SELECT,
          .select = {
              .bofst = 0,
              .sub_ba = &(struct byte_accessor){
                  .type = TA_TAKE,
                  .size = 4,
              },
          },
      },
    };

    ta_memcpy_from (&dest, &src, acc, 2);

    u8 out[5];
    cbuffer_read (out, 5, 1, &dest);
    test_assert_int_equal (out[0], 0xAB);
    test_assert_int_equal (out[1], 78);
    test_assert_int_equal (out[2], 56);
    test_assert_int_equal (out[3], 34);
    test_assert_int_equal (out[4], 12);
  }

  TEST_CASE ("[.a, .b]")
  {
    cbuffer_discard_all (&dest);

    struct byte_accessor acc[] = {
      // SELECT(0, TAKE(4))
      {
          .type = TA_SELECT,
          .select = {
              .bofst = 0,
              .sub_ba = &(struct byte_accessor){
                  .type = TA_TAKE,
                  .size = 4,
              },
          },
      },
      // SELECT(4, TAKE(1))
      {
          .type = TA_SELECT,
          .select = {
              .bofst = 4,
              .sub_ba = &(struct byte_accessor){
                  .type = TA_TAKE,
                  .size = 1,
              },
          },
      },
    };

    cbuffer_discard_all (&dest);
    ta_memcpy_from (&dest, &src, acc, 2);

    u8 out[5];
    cbuffer_read (out, 5, 1, &dest);
    test_assert_int_equal (out[0], 78);
    test_assert_int_equal (out[1], 56);
    test_assert_int_equal (out[2], 34);
    test_assert_int_equal (out[3], 12);
    test_assert_int_equal (out[4], 0xAB);
  }

  TEST_CASE ("[.b.c[1:1:4]]")
  {
    cbuffer_discard_all (&dest);

    struct byte_accessor acc[] = {
      // SELECT(4, SELECT(1, RANGE(1, 1, 4, TAKE(2)))
      {
          .type = TA_SELECT,
          .select = {
              .bofst = 4,
              .sub_ba = &(struct byte_accessor){
                  .type = TA_SELECT,
                  .select = {
                      .bofst = 1,
                      .sub_ba = &(struct byte_accessor){
                          .type = TA_RANGE,
                          .range = {
                              .sub_ba = &(struct byte_accessor){
                                  .type = TA_TAKE,
                                  .size = 2,
                              },
                              .bofst = 1,
                              .stride = 1,
                              .nelems = 4,
                          },
                      },
                  },
              },
          },
      },
    };

    ta_memcpy_from (&dest, &src, acc, 1);

    u8 range_out[6];
    cbuffer_read (range_out, 1, 6, &dest);
    test_assert_int_equal (range_out[0], 2); // c[1] = 2
    test_assert_int_equal (range_out[1], 0);
    test_assert_int_equal (range_out[2], 3); // c[2] = 3
    test_assert_int_equal (range_out[3], 0);
    test_assert_int_equal (range_out[4], 4); // c[3] = 4
    test_assert_int_equal (range_out[5], 0);
  }

  TEST_CASE ("[.b.c[0:2:5]]")
  {
    cbuffer_discard_all (&dest);

    struct byte_accessor acc[] = {
      // SELECT(4, SELECT(1, RANGE(0, 2, 5, TAKE(2))))
      {
          .type = TA_SELECT,
          .select = {
              .bofst = 4,
              .sub_ba = &(struct byte_accessor){
                  .type = TA_SELECT,
                  .select = {
                      .bofst = 1,
                      .sub_ba = &(struct byte_accessor){
                          .type = TA_RANGE,
                          .range = {
                              .sub_ba = &(struct byte_accessor){
                                  .type = TA_TAKE,
                                  .size = 2,
                              },
                              .bofst = 0,
                              .stride = 2,
                              .nelems = 5,
                          },
                      },
                  },
              },
          },
      },
    };

    ta_memcpy_from (&dest, &src, acc, 1);

    u8 out[6];
    cbuffer_read (out, 1, 6, &dest);
    test_assert_int_equal (out[0], 1); // c[0] = 1
    test_assert_int_equal (out[1], 0);
    test_assert_int_equal (out[2], 3); // c[2] = 3
    test_assert_int_equal (out[3], 0);
    test_assert_int_equal (out[4], 5); // c[4] = 5
    test_assert_int_equal (out[5], 0);
  }

  TEST_CASE ("[.a, .a]")
  {
    // a int
    src = cbuffer_create (src_buf, 12);
    dest = cbuffer_create (dest_buf, 12);

    ASSERT (arrlen (test_data) >= 12);
    for (u32 i = 0; i < 12; i++)
      {
        test_data[i] = i;
      }
    cbuffer_write (test_data, 1, 12, &src);

    // Two accessors: both take first 4 bytes
    struct byte_accessor acc[] = {
      {
          .type = TA_TAKE,
          .size = 4,
      },
      {
          .type = TA_TAKE,
          .size = 4,
      },
    };

    ta_memcpy_from (&dest, &src, acc, 2);

    u8 out[8];
    u32 read = cbuffer_read (out, 1, 8, &dest);
    test_assert_int_equal (read, 8);
    test_assert_int_equal (out[0], 0);
    test_assert_int_equal (out[1], 1);
    test_assert_int_equal (out[2], 2);
    test_assert_int_equal (out[3], 3);
    test_assert_int_equal (out[4], 0);
    test_assert_int_equal (out[5], 1);
    test_assert_int_equal (out[6], 2);
    test_assert_int_equal (out[7], 3);
  }

  TEST_CASE ("ba_byte_size")
  {
    struct byte_accessor take = {
      .type = TA_TAKE,
      .size = 8,
    };
    test_assert_int_equal (ba_byte_size (&take), 8);

    struct byte_accessor select = {
      .type = TA_SELECT,
      .select = {
          .bofst = 100,
          .sub_ba = &take,
      },
    };
    test_assert_int_equal (ba_byte_size (&select), 8);

    struct byte_accessor range = {
      .type = TA_RANGE,
      .range = {
          .sub_ba = &take,
          .bofst = 0,
          .stride = 2,
          .nelems = 10,
      },
    };
    test_assert_int_equal (ba_byte_size (&range), 5 * 8);
  }
}
#endif

static void
ta_memcpy_to_once (u8 *dest, struct cbuffer *src, struct byte_accessor *acc)
{
  switch (acc->type)
    {
    case TA_TAKE:
      {
        // Read from src, write to dest
        cbuffer_read (dest, 1, acc->size, src);
        return;
      }
    case TA_SELECT:
      {
        // Skip offset bytes in dest, then recursively copy the selected field
        ta_memcpy_to_once (dest + acc->select.bofst, src, acc->select.sub_ba);
        return;
      }
    case TA_RANGE:
      {
        t_size elem_size = ba_byte_size (acc->range.sub_ba);

        // Calculate starting position in dest
        u8 *dest_pos = dest + (acc->range.bofst * elem_size);

        t_size pos = acc->range.bofst;
        while (pos < acc->range.nelems)
          {
            // Copy one element from src to current dest position
            ta_memcpy_to_once (dest_pos, src, acc->range.sub_ba);

            // Advance dest position by stride elements
            dest_pos += acc->range.stride * elem_size;
            pos += acc->range.stride;
          }

        return;
      }
    }
  UNREACHABLE ();
}

void
ta_memcpy_to (u8 *dest, struct cbuffer *src, struct byte_accessor *acc, u32 acclen)
{
  for (u32 i = 0; i < acclen; i++)
    {
      ta_memcpy_to_once (dest, src, &acc[i]);
    }
}

#ifndef NTEST
TEST (TT_UNIT, ta_memcpy_to_basic)
{
  // struct { a int, b struct { b char, c [5]u16 } }
  u8 src_buf[64];
  u8 dest_buf[64];
  struct cbuffer src = cbuffer_create (src_buf, 64);

  TEST_CASE ("[.a]")
  {
    memset (dest_buf, 0, 64);
    src = cbuffer_create (src_buf, 64);

    // Source: just 4 bytes representing field .a
    u8 test_data[] = { 78, 56, 34, 12 };
    cbuffer_write (test_data, 1, sizeof (test_data), &src);

    struct byte_accessor acc[] = {
      // SELECT(0, TAKE(4))
      {
          .type = TA_SELECT,
          .select = {
              .bofst = 0,
              .sub_ba = &(struct byte_accessor){
                  .type = TA_TAKE,
                  .size = 4,
              },
          },
      },
    };

    ta_memcpy_to (dest_buf, &src, acc, 1);

    test_assert_int_equal (dest_buf[0], 78);
    test_assert_int_equal (dest_buf[1], 56);
    test_assert_int_equal (dest_buf[2], 34);
    test_assert_int_equal (dest_buf[3], 12);
  }

  TEST_CASE ("[.b]")
  {
    memset (dest_buf, 0, 64);
    src = cbuffer_create (src_buf, 64);

    // Source: just 1 byte representing field .b
    u8 test_data[] = { 0xAB };
    cbuffer_write (test_data, 1, sizeof (test_data), &src);

    struct byte_accessor acc[] = {
      // SELECT(4, TAKE(1))
      {
          .type = TA_SELECT,
          .select = {
              .bofst = 4,
              .sub_ba = &(struct byte_accessor){
                  .type = TA_TAKE,
                  .size = 1,
              },
          },
      },
    };

    ta_memcpy_to (dest_buf, &src, acc, 1);

    test_assert_int_equal (dest_buf[4], 0xAB);
  }

  TEST_CASE ("[.b, .a]")
  {
    memset (dest_buf, 0, 64);
    src = cbuffer_create (src_buf, 64);

    u8 test_data[] = {
      0xAB,          // .b
      78, 56, 34, 12 // .a
    };
    cbuffer_write (test_data, 1, sizeof (test_data), &src);

    struct byte_accessor acc[] = {
      // SELECT(4, TAKE(1))
      {
          .type = TA_SELECT,
          .select = {
              .bofst = 4,
              .sub_ba = &(struct byte_accessor){
                  .type = TA_TAKE,
                  .size = 1,
              },
          },
      },
      // SELECT(0, TAKE(4))
      {
          .type = TA_SELECT,
          .select = {
              .bofst = 0,
              .sub_ba = &(struct byte_accessor){
                  .type = TA_TAKE,
                  .size = 4,
              },
          },
      },
    };

    ta_memcpy_to (dest_buf, &src, acc, 2);

    test_assert_int_equal (dest_buf[0], 78); // .a
    test_assert_int_equal (dest_buf[1], 56);
    test_assert_int_equal (dest_buf[2], 34);
    test_assert_int_equal (dest_buf[3], 12);
    test_assert_int_equal (dest_buf[4], 0xAB); // .b
  }

  TEST_CASE ("[.a, .b]")
  {
    memset (dest_buf, 0, 64);
    src = cbuffer_create (src_buf, 64);

    // Source: 4 bytes for .a, then 1 byte for .b (LINEAR in source)
    u8 test_data[] = {
      78, 56, 34, 12, // .a
      0xAB            // .b
    };
    cbuffer_write (test_data, 1, sizeof (test_data), &src);

    struct byte_accessor acc[] = {
      // SELECT(0, TAKE(4))
      {
          .type = TA_SELECT,
          .select = {
              .bofst = 0,
              .sub_ba = &(struct byte_accessor){
                  .type = TA_TAKE,
                  .size = 4,
              },
          },
      },
      // SELECT(4, TAKE(1))
      {
          .type = TA_SELECT,
          .select = {
              .bofst = 4,
              .sub_ba = &(struct byte_accessor){
                  .type = TA_TAKE,
                  .size = 1,
              },
          },
      },
    };

    ta_memcpy_to (dest_buf, &src, acc, 2);

    test_assert_int_equal (dest_buf[0], 78);
    test_assert_int_equal (dest_buf[1], 56);
    test_assert_int_equal (dest_buf[2], 34);
    test_assert_int_equal (dest_buf[3], 12);
    test_assert_int_equal (dest_buf[4], 0xAB);
  }

  TEST_CASE ("[.b.c[1:1:4]]")
  {
    memset (dest_buf, 0, 64);
    src = cbuffer_create (src_buf, 64);

    // Source: 3 u16 values in LINEAR order (c[1], c[2], c[3])
    u8 test_data[] = {
      2, 0, // c[1] = 2
      3, 0, // c[2] = 3
      4, 0, // c[3] = 4
    };
    cbuffer_write (test_data, 1, sizeof (test_data), &src);

    struct byte_accessor acc[] = {
      // SELECT(4, SELECT(1, RANGE(1, 1, 4, TAKE(2))))
      {
          .type = TA_SELECT,
          .select = {
              .bofst = 4,
              .sub_ba = &(struct byte_accessor){
                  .type = TA_SELECT,
                  .select = {
                      .bofst = 1,
                      .sub_ba = &(struct byte_accessor){
                          .type = TA_RANGE,
                          .range = {
                              .sub_ba = &(struct byte_accessor){
                                  .type = TA_TAKE,
                                  .size = 2,
                              },
                              .bofst = 1,
                              .stride = 1,
                              .nelems = 4,
                          },
                      },
                  },
              },
          },
      },
    };

    ta_memcpy_to (dest_buf, &src, acc, 1);

    // offset 4 + 1 + (1*2, 2*2, 3*2) = positions 7, 9, 11
    test_assert_int_equal (dest_buf[7], 2); // c[1]
    test_assert_int_equal (dest_buf[8], 0);
    test_assert_int_equal (dest_buf[9], 3); // c[2]
    test_assert_int_equal (dest_buf[10], 0);
    test_assert_int_equal (dest_buf[11], 4); // c[3]
    test_assert_int_equal (dest_buf[12], 0);
  }

  TEST_CASE ("[.b.c[0:2:5]]")
  {
    memset (dest_buf, 0, 64);
    src = cbuffer_create (src_buf, 64);

    // Source: 3 u16 values in LINEAR order (c[0], c[2], c[4])
    u8 test_data[] = {
      1, 0, // c[0] = 1
      3, 0, // c[2] = 3
      5, 0, // c[4] = 5
    };
    cbuffer_write (test_data, 1, sizeof (test_data), &src);

    struct byte_accessor acc[] = {
      // SELECT(4, SELECT(1, RANGE(0, 2, 5, TAKE(2))))
      {
          .type = TA_SELECT,
          .select = {
              .bofst = 4,
              .sub_ba = &(struct byte_accessor){
                  .type = TA_SELECT,
                  .select = {
                      .bofst = 1,
                      .sub_ba = &(struct byte_accessor){
                          .type = TA_RANGE,
                          .range = {
                              .sub_ba = &(struct byte_accessor){
                                  .type = TA_TAKE,
                                  .size = 2,
                              },
                              .bofst = 0,
                              .stride = 2,
                              .nelems = 5,
                          },
                      },
                  },
              },
          },
      },
    };

    ta_memcpy_to (dest_buf, &src, acc, 1);

    // offset 4 + 1 + (0*2, 2*2, 4*2) = positions 5, 9, 13
    test_assert_int_equal (dest_buf[5], 1); // c[0]
    test_assert_int_equal (dest_buf[6], 0);
    test_assert_int_equal (dest_buf[9], 3); // c[2]
    test_assert_int_equal (dest_buf[10], 0);
    test_assert_int_equal (dest_buf[13], 5); // c[4]
    test_assert_int_equal (dest_buf[14], 0);
  }

  TEST_CASE ("[.a, .a]")
  {
    memset (dest_buf, 0, 64);
    src = cbuffer_create (src_buf, 64);

    // Source: 8 bytes in LINEAR order (two .a fields)
    u8 test_data[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    cbuffer_write (test_data, 1, 8, &src);

    struct byte_accessor acc[] = {
      {
          .type = TA_TAKE,
          .size = 4,
      },
      {
          .type = TA_TAKE,
          .size = 4,
      },
    };

    ta_memcpy_to (dest_buf, &src, acc, 2);

    // First 4 source bytes go to dest[0..3], then reset src
    // Next 4 bytes OVERWRITE dest[0..3]
    test_assert_int_equal (dest_buf[0], 4); // Second write wins
    test_assert_int_equal (dest_buf[1], 5);
    test_assert_int_equal (dest_buf[2], 6);
    test_assert_int_equal (dest_buf[3], 7);
  }
}
#endif
