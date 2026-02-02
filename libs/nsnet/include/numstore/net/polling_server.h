#pragma once

#include "numstore/core/latch.h"
#include <numstore/core/error.h>
#include <numstore/core/slab_alloc.h>
#include <sys/poll.h>

struct connection
{
  void *rx_buf;
  u32 rx_cap;
  u32 rx_len;
  void *tx_buf;
  u32 tx_cap;
  u32 tx_sent;
  int fd;
  struct latch l;
};

struct conn_actions
{
  struct connection *(*conn_alloc) (void *ctx, error *e);
  err_t (*conn_func) (void *ctx, struct connection *conn, error *e);
  void (*conn_free) (void *ctx, struct connection *conn);
};

struct polling_server
{
  struct pollfd *fds;
  struct connection **conns;
  u32 len;
  u32 cap;
  volatile int running;

  struct conn_actions actions;

  void *ctx;
};

err_t pserv_open (struct polling_server *ps, int port, struct conn_actions actions, void *ctx, error *e);
int pserv_execute (struct polling_server *ps, error *e);
err_t pserv_close (struct polling_server *ps, error *e);
