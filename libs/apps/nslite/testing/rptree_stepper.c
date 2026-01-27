#include <numstore/test/rptree_stepper.h>
#include <numstore/test/testing.h>

static u32
rand_next (u32 *seed)
{
  *seed = (*seed * 1103515245 + 12345) & 0x7fffffff;
  return *seed;
}

static u32
rand_range (u32 *seed, u32 min, u32 max)
{
  if (min >= max)
    {
      return min;
    }
  return min + (rand_next (seed) % (max - min));
}

err_t
rptv_stepper_open (struct rptv_stepper *s, const char *fname, const char *recovery, u32 seed, error *e)
{
  s->v = rptv_open (fname, recovery, e);
  if (!s->v)
    {
      return e->cause_code;
    }

  spgno pg = rptv_new (s->v, e);
  if (pg < 0)
    {
      rptv_close (s->v, e);
      return e->cause_code;
    }

  s->current_page = (pgno)pg;
  s->step_count = 0;
  s->seed = seed ? seed : (u32)time (NULL);

  return SUCCESS;
}

err_t
rptv_stepper_close (struct rptv_stepper *s, error *e)
{
  err_t ret = rptv_close (s->v, e);
  return ret;
}

static err_t
execute_insert (struct rptv_stepper *s, b_size bofst, b_size nelem, error *e)
{
  u8 *data = i_malloc (1, nelem, e);
  if (!data)
    {
      return e->cause_code;
    }

  // Fill with random data
  for (b_size i = 0; i < nelem; ++i)
    {
      data[i] = (u8)rand_next (&s->seed);
    }

  err_t ret = rptv_insert (s->v, s->current_page, data, bofst, 1, nelem, e);
  i_free (data);
  return ret;
}

static err_t
execute_write (struct rptv_stepper *s, b_size bstart, b_size nelem, u32 stride, error *e)
{
  sb_size size = rptv_size (s->v, s->current_page, e);
  if (size < 0)
    {
      return e->cause_code;
    }

  if (size == 0)
    {
      return SUCCESS; // Can't write to empty buffer
    }

  // Ensure we don't write past the end
  b_size max_offset = (b_size)size;
  if (bstart >= max_offset)
    {
      bstart = max_offset > 0 ? max_offset - 1 : 0;
    }

  // Adjust nelem if necessary
  b_size available = (max_offset - bstart + stride - 1) / stride;
  if (nelem > available)
    {
      nelem = available;
    }

  if (nelem == 0)
    {
      return SUCCESS;
    }

  u8 *data = i_malloc (1, nelem, e);
  if (!data)
    {
      return e->cause_code;
    }

  for (b_size i = 0; i < nelem; ++i)
    {
      data[i] = (u8)rand_next (&s->seed);
    }

  struct nslite_stride str = {
    .bstart = bstart,
    .stride = stride,
    .nelems = nelem,
  };

  err_t ret = rptv_write (s->v, s->current_page, data, 1, str, e);
  i_free (data);
  return ret;
}

static err_t
execute_remove (struct rptv_stepper *s, b_size bstart, b_size nelem, u32 stride, error *e)
{
  sb_size size = rptv_size (s->v, s->current_page, e);
  if (size < 0)
    {
      return e->cause_code;
    }

  if (size == 0)
    {
      return SUCCESS; // Nothing to remove
    }

  b_size max_offset = (b_size)size;
  if (bstart >= max_offset)
    {
      bstart = max_offset > 0 ? max_offset - 1 : 0;
    }

  // Adjust nelem if necessary
  b_size available = (max_offset - bstart + stride - 1) / stride;
  if (nelem > available)
    {
      nelem = available;
    }

  if (nelem == 0)
    {
      return SUCCESS;
    }

  u8 *data = i_malloc (1, nelem, e);
  if (!data)
    {
      return e->cause_code;
    }

  struct nslite_stride str = {
    .bstart = bstart,
    .stride = stride,
    .nelems = nelem,
  };

  err_t ret = rptv_remove (s->v, s->current_page, data, 1, str, e);
  i_free (data);
  return ret;
}

static err_t
execute_read (struct rptv_stepper *s, b_size bstart, b_size nelem, u32 stride, error *e)
{
  sb_size size = rptv_size (s->v, s->current_page, e);
  if (size < 0)
    {
      return e->cause_code;
    }

  if (size == 0)
    {
      return SUCCESS;
    }

  b_size max_offset = (b_size)size;
  if (bstart >= max_offset)
    {
      return SUCCESS;
    }

  b_size available = (max_offset - bstart + stride - 1) / stride;
  if (nelem > available)
    {
      nelem = available;
    }

  if (nelem == 0)
    {
      return SUCCESS;
    }

  u8 *data = i_malloc (1, nelem, e);
  if (!data)
    {
      return e->cause_code;
    }

  struct nslite_stride str = {
    .bstart = bstart,
    .stride = stride,
    .nelems = nelem,
  };

  sb_size read = rptv_read (s->v, s->current_page, data, 1, str, e);
  i_free (data);
  if (read < 0)
    {
      return e->cause_code;
    }
  return SUCCESS;
}

err_t
rptv_stepper_execute (struct rptv_stepper *s, error *e)
{
  enum rptv_move_type move = rand_range (&s->seed, 0, RPTV_MOVE_COUNT);

  sb_size size = rptv_size (s->v, s->current_page, e);
  if (size < 0)
    {
      return e->cause_code;
    }

  b_size current_size = (b_size)size;
  err_t ret = SUCCESS;

  switch (move)
    {
    case RPTV_MOVE_INSERT_SINGLE_START:
      {
        ret = execute_insert (s, 0, 1, e);
        break;
      }

    case RPTV_MOVE_INSERT_SINGLE_END:
      {
        ret = execute_insert (s, current_size, 1, e);
        break;
      }

    case RPTV_MOVE_INSERT_SINGLE_MIDDLE:
      {
        if (current_size > 0)
          {
            ret = execute_insert (s, rand_range (&s->seed, 0, current_size), 1, e);
          }
        else
          {
            ret = execute_insert (s, 0, 1, e);
          }
        break;
      }

    case RPTV_MOVE_INSERT_SMALL_START:
      {
        ret = execute_insert (s, 0, rand_range (&s->seed, 1, 11), e);
        break;
      }

    case RPTV_MOVE_INSERT_SMALL_END:
      {
        ret = execute_insert (s, current_size, rand_range (&s->seed, 1, 11), e);
        break;
      }

    case RPTV_MOVE_INSERT_SMALL_MIDDLE:
      {
        if (current_size > 0)
          {
            ret = execute_insert (s, rand_range (&s->seed, 0, current_size), rand_range (&s->seed, 1, 11), e);
          }
        else
          {
            ret = execute_insert (s, 0, rand_range (&s->seed, 1, 11), e);
          }
        break;
      }

    case RPTV_MOVE_INSERT_MEDIUM:
      {
        if (current_size > 0)
          {
            ret = execute_insert (s, rand_range (&s->seed, 0, current_size), rand_range (&s->seed, 10, 10001), e);
          }
        else
          {
            ret = execute_insert (s, 0, rand_range (&s->seed, 10, 10001), e);
          }
        break;
      }

    case RPTV_MOVE_INSERT_LARGE:
      {
        if (current_size > 0)
          {
            ret = execute_insert (s, rand_range (&s->seed, 0, current_size), rand_range (&s->seed, 1000, 10000), e);
          }
        else
          {
            ret = execute_insert (s, 0, rand_range (&s->seed, 100, 10000), e);
          }
        break;
      }

    case RPTV_MOVE_WRITE_SINGLE_START:
      {
        ret = execute_write (s, 0, 1, 1, e);
        break;
      }

    case RPTV_MOVE_WRITE_SINGLE_END:
      {
        if (current_size > 0)
          {
            ret = execute_write (s, current_size - 1, 1, 1, e);
          }
        break;
      }

    case RPTV_MOVE_WRITE_SINGLE_MIDDLE:
      {
        if (current_size > 0)
          {
            ret = execute_write (s, rand_range (&s->seed, 0, current_size), 1, 1, e);
          }
        break;
      }

    case RPTV_MOVE_WRITE_SMALL_START:
      {
        ret = execute_write (s, 0, rand_range (&s->seed, 1, 11), 1, e);
        break;
      }

    case RPTV_MOVE_WRITE_SMALL_END:
      {
        if (current_size >= 10)
          {
            ret = execute_write (s, current_size - 10, rand_range (&s->seed, 1, 11), 1, e);
          }
        else if (current_size > 0)
          {
            ret = execute_write (s, 0, current_size, 1, e);
          }
        break;
      }

    case RPTV_MOVE_WRITE_SMALL_MIDDLE:
      {
        if (current_size > 0)
          {
            ret = execute_write (s, rand_range (&s->seed, 0, current_size),
                                 rand_range (&s->seed, 1, 11), 1, e);
          }
        break;
      }

    case RPTV_MOVE_WRITE_MEDIUM:
      {
        if (current_size > 0)
          {
            ret = execute_write (s, rand_range (&s->seed, 0, current_size),
                                 rand_range (&s->seed, 10, 10001), 1, e);
          }
        break;
      }

    case RPTV_MOVE_WRITE_LARGE:
      {
        if (current_size > 0)
          {
            ret = execute_write (s, rand_range (&s->seed, 0, current_size),
                                 rand_range (&s->seed, 100, 10000), 1, e);
          }
        break;
      }

    case RPTV_MOVE_WRITE_STRIDED:
      {
        if (current_size > 10)
          {
            ret = execute_write (s, rand_range (&s->seed, 0, current_size / 2),
                                 rand_range (&s->seed, 5, 21),
                                 rand_range (&s->seed, 2, 6), e);
          }
        break;
      }

    case RPTV_MOVE_REMOVE_SINGLE_START:
      {
        ret = execute_remove (s, 0, 1, 1, e);
        break;
      }

    case RPTV_MOVE_REMOVE_SINGLE_END:
      {
        if (current_size > 0)
          {
            ret = execute_remove (s, current_size - 1, 1, 1, e);
          }
        break;
      }

    case RPTV_MOVE_REMOVE_SINGLE_MIDDLE:
      {
        if (current_size > 0)
          {
            ret = execute_remove (s, rand_range (&s->seed, 0, current_size), 1, 1, e);
          }
        break;
      }

    case RPTV_MOVE_REMOVE_SMALL_START:
      {
        ret = execute_remove (s, 0, rand_range (&s->seed, 1, 11), 1, e);
        break;
      }

    case RPTV_MOVE_REMOVE_SMALL_END:
      {
        if (current_size > 0)
          {
            b_size nelem = rand_range (&s->seed, 1, 11);
            b_size start = current_size > nelem ? current_size - nelem : 0;
            ret = execute_remove (s, start, nelem, 1, e);
          }
        break;
      }

    case RPTV_MOVE_REMOVE_SMALL_MIDDLE:
      {
        if (current_size > 0)
          {
            ret = execute_remove (s, rand_range (&s->seed, 0, current_size),
                                  rand_range (&s->seed, 1, 11), 1, e);
          }
        break;
      }

    case RPTV_MOVE_REMOVE_MEDIUM:
      {
        if (current_size > 0)
          {
            ret = execute_remove (s, rand_range (&s->seed, 0, current_size),
                                  rand_range (&s->seed, 100, 10001), 1, e);
          }
        break;
      }

    case RPTV_MOVE_REMOVE_LARGE:
      {
        if (current_size > 0)
          {
            ret = execute_remove (
                s, rand_range (&s->seed, 0, current_size), rand_range (&s->seed, 10000, 10000), 1, e);
          }
        break;
      }

    case RPTV_MOVE_REMOVE_STRIDED:
      {
        if (current_size > 10)
          {
            ret = execute_remove (s, rand_range (&s->seed, 0, current_size / 2),
                                  rand_range (&s->seed, 5, 21),
                                  rand_range (&s->seed, 2, 6), e);
          }
        break;
      }

    case RPTV_MOVE_READ_FULL:
      {
        ret = execute_read (s, 0, current_size, 1, e);
        break;
      }

    case RPTV_MOVE_READ_STRIDED:
      {
        if (current_size > 10)
          {
            ret = execute_read (s, rand_range (&s->seed, 0, current_size / 2),
                                rand_range (&s->seed, 5, 21),
                                rand_range (&s->seed, 2, 6), e);
          }
        break;
      }

    default:
      {
        break;
      }
    }

  if (ret == SUCCESS)
    {
      s->step_count++;
    }

  return ret;
}
