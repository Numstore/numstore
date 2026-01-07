#pragma once

#include <numstore/rptree/rptree_cursor.h>

err_t rptof_insert (
    struct rptree_cursor *c,
    const void *src,
    size_t bofst,
    size_t size,
    size_t nelem,
    error *e);

err_t rptof_write (
    struct rptree_cursor *c,
    const void *src,
    t_size size,
    b_size bstart,
    u32 stride,
    b_size nelems,
    error *e);

sb_size rptof_read (
    struct rptree_cursor *c,
    void *dest,
    t_size size,
    b_size bstart,
    u32 stride,
    b_size nelems,
    error *e);

err_t rptof_remove (
    struct rptree_cursor *c,
    void *dest,
    t_size size,
    b_size bstart,
    u32 stride,
    b_size nelems,
    error *e);
