#pragma once

#include <nslite.h>
#include <numstore/test/rptree_mem.h>

struct rptree_validator
{
  nslite *ns;
  struct rptree_mem *mem;
};

struct rptree_validator *rptv_open (const char *fname, const char *recovery, error *e);
err_t rptv_close (struct rptree_validator *v, error *e);
spgno rptv_new (struct rptree_validator *v, error *e);
err_t rptv_delete (struct rptree_validator *v, pgno start, error *e);
sb_size rptv_size (struct rptree_validator *v, pgno id, error *e);
err_t rptv_insert (
    struct rptree_validator *v,
    pgno id,
    const void *src,
    b_size bofst,
    t_size size,
    b_size nelem,
    error *e);
err_t rptv_write (
    struct rptree_validator *v,
    pgno id,
    const void *src,
    t_size size,
    struct nslite_stride stride,
    error *e);
sb_size rptv_read (
    struct rptree_validator *v,
    pgno id,
    void *dest,
    t_size size,
    struct nslite_stride stride,
    error *e);
err_t rptv_remove (
    struct rptree_validator *v,
    pgno id,
    void *dest,
    t_size size,
    struct nslite_stride stride,
    error *e);
