#include "numstore/core/assert.h"
#include "numstore/core/chunk_alloc.h"
#include "numstore/rs/rptrs.h"
#include "numstore/rs/trfmrs.h"
#include "numstore/types/type_accessor.h"
#include <numstore/repository/nsdb_rp.h>
#include <numstore/rs/result_set.h>

static inline struct result_set *
rs_base_rs (
    struct string vname,
    struct chunk_alloc *persistent,
    struct nsdb_rp *ds,
    struct user_stride stride,
    error *e)
{
  // Construct the base result set
  struct rptree_cursor *rc = nsdb_rp_open_cursor (ds, vname, e);
  if (rc == NULL)
    {
      return NULL;
    }
  const struct variable *var = nsdb_rp_get_variable (ds, vname, e);
  if (var == NULL)
    {
      nsdb_rp_close_cursor (ds, rc, e);
      return NULL;
    }

  struct result_set *rptc = rptrs_create (rc, persistent, stride, var->dtype, e);
  if (rptc == NULL)
    {
      nsdb_rp_close_cursor (ds, rc, e);
      nsdb_rp_free_variable (ds, var, e);
      return NULL;
    }

  return rptc;
}

struct result_set *
nsdb_rp_get_result_set (
    struct nsdb_rp *ds,
    struct type_ref *tr,
    struct chunk_alloc *persistent,
    struct user_stride stride,
    error *e)
{
  switch (tr->type)
    {
    case TR_TAKE:
      {
        switch (tr->tk.ta.type)
          {
            // read a
          case TA_TAKE:
            {
              return rs_base_rs (tr->tk.vname, persistent, ds, stride, e);
            }

            // read a.b.c
          case TA_SELECT:

            // read a[0:10]
          case TA_RANGE:
            {
              // First - open the new cursor with full stride
              struct result_set *base = rs_base_rs (tr->tk.vname, persistent, ds, USER_STRIDE_ALL, e);
              if (base == NULL)
                {
                  return NULL;
                }

              // Then build a transform result set filter
              struct trfmrsb trb;
              struct chunk_alloc temp;
              chunk_alloc_create_default (&temp);
              trfmrsb_create (&trb, &temp, persistent);

              // Add one result set select
              if (trfmrsb_append_select (&trb, base, &tr->tk.ta, tr->tk.vname, e))
                {
                  chunk_alloc_free_all (&temp);
                  return NULL;
                }
              if (trfmrsb_add_slice (&trb, stride, e))
                {
                  chunk_alloc_free_all (&temp);
                  return NULL;
                }

              chunk_alloc_free_all (&temp);
              return trfmrsb_build (&trb, e);
            }
          }
        UNREACHABLE ();
      }
    case TR_STRUCT:
      {
        struct trfmrsb trb;
        struct chunk_alloc temp;
        chunk_alloc_create_default (&temp);
        trfmrsb_create (&trb, &temp, persistent);

        for (u32 i = 0; i < tr->st.len; ++i)
          {
            struct result_set *rs = nsdb_rp_get_result_set (ds, &tr->st.types[i], persistent, USER_STRIDE_ALL, e);
            if (rs == NULL)
              {
                chunk_alloc_free_all (&temp);
                return NULL;
              }

            struct type_accessor *ta = chunk_malloc (persistent, 1, sizeof *ta, e);
            if (ta == NULL)
              {
                chunk_alloc_free_all (&temp);
                return NULL;
              }
            ta->type = TA_TAKE;

            trfmrsb_append_select (&trb, rs, ta, tr->st.keys[i], e);
          }

        if (trfmrsb_add_slice (&trb, stride, e))
          {
            chunk_alloc_free_all (&temp);
            return NULL;
          }

        return trfmrsb_build (&trb, e);
      }
    }

  UNREACHABLE ();
}

err_t
rs_execute (struct result_set *rs, error *e)
{
  switch (rs->type)
    {
    case RS_DB:
      {
        return rptrs_execute (rs, e);
      }
    case RS_TRANSFORM:
      {
        return trfmrs_execute (rs, e);
      }
    }
  UNREACHABLE ();
}
