#include <numstore/test/rptree_validator.h>

static struct rptm_stride
nslite_to_rptm_stride (struct nslite_stride s)
{
  return (struct rptm_stride){
    .bstart = s.bstart,
    .stride = s.stride,
    .nelems = s.nelems,
  };
}

struct rptree_validator *
rptv_open (const char *fname, const char *recovery, error *e)
{
  struct rptree_validator *v = i_malloc (1, sizeof (*v), e);
  if (!v)
    {
      return NULL;
    }

  v->ns = nslite_open (fname, recovery, e);
  if (!v->ns)
    {
      i_free (v);
      return NULL;
    }

  v->mem = rptm_open (e);
  if (!v->mem)
    {
      nslite_close (v->ns, e);
      i_free (v);
      return NULL;
    }

  return v;
}

err_t
rptv_close (struct rptree_validator *v, error *e)
{
  err_t ret = nslite_close (v->ns, e);
  rptm_close (v->mem);
  i_free (v);
  return ret;
}

spgno
rptv_new (struct rptree_validator *v, error *e)
{
  spgno pg = nslite_new (v->ns, NULL, e);
  if (pg < 0)
    {
      return pg;
    }

  if (rptm_new (v->mem, (pgno)pg, e))
    {
      return e->cause_code;
    }

  return pg;
}

err_t
rptv_delete (struct rptree_validator *v, pgno start, error *e)
{
  err_t_wrap (nslite_delete (v->ns, NULL, start, e), e);
  rptm_delete (v->mem, start);
  return SUCCESS;
}

sb_size
rptv_size (struct rptree_validator *v, pgno id, error *e)
{

  sb_size ns_size = nslite_size (v->ns, id, e);
  if (ns_size < 0)
    {
      return ns_size;
    }

  b_size mem_size = rptm_size (v->mem, id);
  if ((b_size)ns_size != mem_size)
    {
      return error_causef (
          e, ERR_FAILED_TEST,
          "Size mismatch: nslite=%" PRb_size ", rptree_mem=%" PRb_size,
          ns_size, mem_size);
    }

  i_log_debug ("Validator size. pgno: %" PRpgno " size: %" PRb_size "\n", id, mem_size);

  return ns_size;
}

err_t
rptv_insert (
    struct rptree_validator *v,
    pgno id,
    const void *src,
    b_size bofst,
    t_size size,
    b_size nelem,
    error *e)
{
  i_log_debug ("Validator inserting. pgno: %" PRpgno " nelem: %" PRb_size
               " at offset: %" PRb_size " Elements of size: %" PRt_size "\n",
               id, nelem, bofst, size);

  // Perform insert on both
  err_t_wrap (nslite_insert (v->ns, id, NULL, src, bofst, size, nelem, e), e);

  err_t_wrap (rptm_insert (v->mem, id, src, bofst, size, nelem, e), e);

  // Validate by reading back from nslite
  b_size total_bytes = size * nelem;
  void *ns_buf = i_malloc (1, total_bytes, e);
  void *mem_buf = i_malloc (1, total_bytes, e);

  if (!ns_buf || !mem_buf)
    {
      i_cfree (ns_buf);
      i_cfree (mem_buf);
      return e->cause_code;
    }

  struct nslite_stride stride = {
    .bstart = bofst,
    .stride = 1,
    .nelems = nelem,
  };

  sb_size ns_read = nslite_read (v->ns, id, ns_buf, size, stride, e);
  if (ns_read < 0)
    {
      i_free (ns_buf);
      i_free (mem_buf);
      return e->cause_code;
    }

  b_size mem_read = rptm_read (v->mem, id, mem_buf, size, nslite_to_rptm_stride (stride));

  if ((b_size)ns_read != mem_read || (b_size)ns_read != total_bytes)
    {
      i_free (ns_buf);
      i_free (mem_buf);
      return error_causef (
          e, ERR_FAILED_TEST,
          "Insert validation read length mismatch: nslite=%" PRb_size ", mem=%" PRb_size ", expected=%" PRb_size,
          ns_read, mem_read, total_bytes);
    }

  if (i_memcmp (ns_buf, mem_buf, total_bytes) != 0)
    {
      i_free (ns_buf);
      i_free (mem_buf);
      return error_causef (e, ERR_FAILED_TEST, "Insert validation: data mismatch between nslite and rptree_mem");
    }

  i_free (ns_buf);
  i_free (mem_buf);

  // Final validation
  return nslite_validate (v->ns, id, e);
}

err_t
rptv_write (
    struct rptree_validator *v,
    pgno id,
    const void *src,
    t_size size,
    struct nslite_stride stride,
    error *e)
{
  i_log_debug ("Validator writing. pgno: %" PRpgno " size: %" PRt_size
               " bstart: %" PRb_size " stride: %d nelems: %" PRb_size "\n",
               id, size, stride.bstart, stride.stride, stride.nelems);

  // Perform write on both
  err_t_wrap (nslite_write (v->ns, id, NULL, src, size, stride, e), e);

  b_size mem_written = rptm_write (v->mem, id, src, size,
                                   nslite_to_rptm_stride (stride));

  // Validate by reading back from nslite
  b_size total_bytes = size * stride.nelems;
  void *ns_buf = i_malloc (total_bytes, 1, e);
  void *mem_buf = i_malloc (total_bytes, 1, e);

  if (!ns_buf || !mem_buf)
    {
      i_cfree (ns_buf);
      i_cfree (mem_buf);
      return e->cause_code;
    }

  sb_size ns_read = nslite_read (v->ns, id, ns_buf, size, stride, e);
  if (ns_read < 0)
    {
      i_free (ns_buf);
      i_free (mem_buf);
      return ERR_FAILED_TEST;
    }

  b_size mem_read = rptm_read (v->mem, id, mem_buf, size, nslite_to_rptm_stride (stride));

  if ((b_size)ns_read != mem_read || (b_size)ns_read != total_bytes)
    {
      i_free (ns_buf);
      i_free (mem_buf);
      return error_causef (
          e, ERR_FAILED_TEST,
          "Write validation read length mismatch: nslite=%" PRb_size ", mem=%" PRb_size ", expected=%" PRb_size,
          ns_read, mem_read, total_bytes);
    }

  if (i_memcmp (ns_buf, mem_buf, total_bytes) != 0)
    {
      i_free (ns_buf);
      i_free (mem_buf);
      return error_causef (e, ERR_FAILED_TEST, "Write validation: data mismatch between nslite and rptree_mem");
    }

  i_free (ns_buf);
  i_free (mem_buf);

  // Final validation
  return nslite_validate (v->ns, id, e);
}

sb_size
rptv_read (
    struct rptree_validator *v,
    pgno id,
    void *dest,
    t_size size,
    struct nslite_stride stride,
    error *e)
{
  i_log_debug ("Validator reading. pgno: %" PRpgno " size: %" PRt_size
               " bstart: %" PRb_size " stride: %d nelems: %" PRb_size "\n",
               id, size, stride.bstart, stride.stride, stride.nelems);

  void *mem_buf = i_malloc (size * stride.nelems, 1, e);
  if (!mem_buf)
    {
      return e->cause_code;
    }

  sb_size ns_read = nslite_read (v->ns, id, dest, size, stride, e);
  if (ns_read < 0)
    {
      i_free (mem_buf);
      return ns_read;
    }

  b_size mem_read = rptm_read (v->mem, id, mem_buf, size, nslite_to_rptm_stride (stride));

  if ((b_size)ns_read != mem_read)
    {
      i_free (mem_buf);
      return error_causef (
          e, ERR_FAILED_TEST,
          "Read length mismatch: nslite=%" PRb_size ", mem=%" PRb_size,
          ns_read, mem_read);
    }

  if (i_memcmp (dest, mem_buf, (b_size)ns_read) != 0)
    {
      i_free (mem_buf);
      return error_causef (e, ERR_FAILED_TEST,
                           "Read data mismatch between nslite and rptree_mem");
    }

  i_free (mem_buf);
  return ns_read;
}

err_t
rptv_remove (
    struct rptree_validator *v,
    pgno id,
    void *dest,
    t_size size,
    struct nslite_stride stride,
    error *e)
{
  i_log_debug ("Validator removing. pgno: %" PRpgno " size: %" PRt_size
               " bstart: %" PRb_size " stride: %d nelems: %" PRb_size "\n",
               id, size, stride.bstart, stride.stride, stride.nelems);

  b_size total_bytes = size * stride.nelems;
  void *mem_buf = i_malloc (total_bytes, 1, e);

  if (!mem_buf)
    {
      return e->cause_code;
    }

  // Perform remove on both
  if (nslite_remove (v->ns, id, NULL, dest, size, stride, e))
    {
      i_free (mem_buf);
      return e->cause_code;
    }

  b_size mem_removed = rptm_remove (v->mem, id, mem_buf, size, nslite_to_rptm_stride (stride));

  // Compare what was removed
  if (i_memcmp (dest, mem_buf, total_bytes) != 0)
    {
      i_free (mem_buf);
      return error_causef (e, ERR_FAILED_TEST, "Remove data mismatch between nslite and rptree_mem");
    }

  i_free (mem_buf);

  // Final validation
  return nslite_validate (v->ns, id, e);
}
