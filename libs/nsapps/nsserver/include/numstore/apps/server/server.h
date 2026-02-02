#include "numstore/core/slab_alloc.h"
#include <numstore/net/polling_server.h>

struct connection_manager
{
  struct slab_alloc conn_alloc;
  struct nsfslite *db;
};

struct connection *nsconn_alloc (void *ctx, error *e);
err_t nsconn_func (void *ctx, struct connection *conn, error *e);
void nsconn_free (void *ctx, struct connection *conn, error *e);
