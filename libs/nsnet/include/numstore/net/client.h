#pragma once

#include <numstore/core/error.h>

struct client
{
  int fd;
};

err_t client_connect (struct client *dest, const char *host, u16 port, error *e);
err_t client_write_all (struct client *c, const void *src, u16 len, error *e);
err_t client_write_all_size_prefixed (struct client *c, const void *msg, u16 len, error *e);
err_t client_read_all (struct client *c, void *dest, u16 len, error *e);
i32 client_read_all_size_prefixed (struct client *c, void *dest, u16 len, error *e);
err_t client_disconnect (struct client *c, error *e);
