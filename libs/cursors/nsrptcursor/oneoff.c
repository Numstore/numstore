#include <numstore/rptree/oneoff.h>

err_t
rptof_insert (
    struct rptree_cursor *c,
    const void *src,
    size_t bofst,
    size_t size,
    size_t nelem,
    error *e)
{
  if (bofst > c->total_size)
    {
      return error_causef (
          e, ERR_INVALID_ARGUMENT,
          "Byte range: %" PRb_size " exceeds total length: %" PRb_size "\n",
          bofst, c->total_size);
    }

  b_size nbytes = nelem * size;
  b_size written = 0;

  pgno root_before = c->root;

  while (written < nbytes)
    {
      // SEEK
      err_t_wrap (rptc_start_seek (c, bofst + written, true, e), e);
      while (c->state == RPTS_SEEKING)
        {
          err_t_wrap (rptc_seeking_execute (c, e), e);
        }

      // INSERT
      u32 _nbytes = MIN (nbytes - written, NUPD_MAX_DATA_LENGTH);
      struct cbuffer srcbuf = cbuffer_create_with ((u8 *)src + written, _nbytes, _nbytes);
      err_t_wrap (rptc_seeked_to_insert (c, &srcbuf, _nbytes, e), e);

      while (c->state == RPTS_DL_INSERTING)
        {
          err_t_wrap (rptc_insert_execute (c, e), e);
        }

      while (c->state == RPTS_IN_REBALANCING)
        {
          err_t_wrap (rptc_rebalance_execute (c, e), e);
        }

      written += _nbytes;
    }

  err_t_wrap (rptc_update_meta (c, e), e);

  return SUCCESS;
}

err_t
rptof_write (
    struct rptree_cursor *c,
    const void *src,
    t_size size,
    b_size bstart,
    u32 stride,
    b_size nelems,
    error *e)
{
  if (bstart + nelems * size > c->total_size)
    {
      return error_causef (
          e, ERR_INVALID_ARGUMENT,
          "Byte range: %" PRb_size " exceeds total length: %" PRb_size "\n",
          bstart + nelems * size, c->total_size);
    }

  // SEEK
  err_t_wrap (rptc_start_seek (c, bstart, false, e), e);

  while (c->state == RPTS_SEEKING)
    {
      err_t_wrap (rptc_seeking_execute (c, e), e);
    }

  // WRITE
  u32 nbytes = nelems * size;
  struct cbuffer srcbuf = cbuffer_create_with ((void *)src, nbytes, nbytes);
  err_t_wrap (rptc_seeked_to_write (c, &srcbuf, nelems, size, stride, e), e);

  while (c->state == RPTS_DL_WRITING)
    {
      err_t_wrap (rptc_write_execute (c, e), e);
    }

  ASSERT (c->writer.total_bwritten % size == 0);

  return SUCCESS;
}

sb_size
rptof_read (
    struct rptree_cursor *c,
    void *dest,
    t_size size,
    b_size bstart,
    u32 stride,
    b_size nelems,
    error *e)
{
  // SEEK
  err_t_wrap (rptc_start_seek (c, bstart, false, e), e);
  while (c->state == RPTS_SEEKING)
    {
      err_t_wrap (rptc_seeking_execute (c, e), e);
    }

  // READ
  u32 nbytes = nelems * size;
  struct cbuffer destbuf = cbuffer_create (dest, nbytes);
  rptc_seeked_to_read (c, &destbuf, nelems, size, stride);

  while (c->state == RPTS_DL_READING)
    {
      err_t_wrap (rptc_read_execute (c, e), e);
    }

  ASSERT (c->reader.total_bread % size == 0);

  return c->reader.total_bread / size;
}

err_t
rptof_remove (
    struct rptree_cursor *c,
    void *dest,
    t_size size,
    b_size bstart,
    u32 stride,
    b_size nelems,
    error *e)
{
  if (bstart + nelems * size > c->total_size)
    {
      return error_causef (
          e, ERR_INVALID_ARGUMENT,
          "Byte range: %" PRb_size " exceeds total length: %" PRb_size "\n",
          bstart + nelems * size, c->total_size);
    }

  b_size nbytes = nelems * size;
  b_size removed = 0;

  while (removed < nbytes)
    {
      // SEEK
      err_t_wrap (rptc_start_seek (c, bstart + removed, false, e), e);
      while (c->state == RPTS_SEEKING)
        {
          err_t_wrap (rptc_seeking_execute (c, e), e);
        }

      // REMOVE
      u32 _nbytes = MIN (nbytes - removed, NUPD_MAX_DATA_LENGTH);
      b_size _nelems = _nbytes / size;

      if (dest)
        {
          struct cbuffer destbuf = cbuffer_create ((u8 *)dest + removed, _nbytes);
          err_t_wrap (rptc_seeked_to_remove (c, &destbuf, _nelems, size, stride, e), e);
        }
      else
        {
          err_t_wrap (rptc_seeked_to_remove (c, NULL, _nelems, size, stride, e), e);
        }

      while (c->state == RPTS_DL_REMOVING)
        {
          err_t_wrap (rptc_remove_execute (c, e), e);
        }

      // REBALANCE
      while (c->state == RPTS_IN_REBALANCING)
        {
          err_t_wrap (rptc_rebalance_execute (c, e), e);
        }

      removed += _nbytes;
    }

  err_t_wrap (rptc_update_meta (c, e), e);

  return SUCCESS;
}
