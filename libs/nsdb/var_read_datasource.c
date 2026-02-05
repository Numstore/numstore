#include <numstore/nsdb/var_read_datasource.h>

////////////////////////////////////////////////////////////////
/// Variable meta data source

struct var_source
{
  struct var_cursor cursor;
  struct htable *cache;
  struct chunk_alloc *alloc;

  // Destination for a get var call
  struct type *dtype;
  pgno root;
  b_size nbytes;
};

static err_t
vars_init (struct var_source *dest, struct chunk_alloc *alloc, error *e)
{
}

static void
vars_close (struct var_source *v)
{
}

static err_t
vars_get_var (struct var_source *v, struct string vname, error *e)
{
}

////////////////////////////////////////////////////////////////
/// A single variable data source

struct rpt_var_source
{
  pgno root;
  struct rptree_cursor cursor;
  struct string vname;
  struct cbuffer output;
  struct hnode node;
  u8 _backing_data[];
};

struct rptvs_bank
{
  struct htable *table;
  struct chunk_alloc *alloc;
};

err_t rptvb_init (struct rptvs_bank *dest, struct chunk_alloc *alloc, error *e);
err_t rptvb_close (struct rptvs_bank *bank, error *e);

err_t rptvb_insert (struct rptvs_bank *bank, struct string vname, pgno root, struct type type, error *e);
struct rpt_var_source *rptvb_get (struct rptvs_bank *bank, struct string vname);

err_t rptvb_execute (struct rptvs_bank *bank, error *e);
