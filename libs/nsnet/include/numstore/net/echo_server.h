#pragma once

#include <numstore/core/error.h>
#include <numstore/net/polling_server.h>

struct echo_context
{
  const char *prefix;
  const char *suffix;
};

struct connection *echo_conn_alloc (void *ctx, error *e);
err_t echo_conn_func (void *echo_ctx, struct connection *conn, error *e);
void echo_conn_free (void *ctx, struct connection *conn);
