#include <numstore/core/error.h>
#include <numstore/intf/os.h>
#include <numstore/net/client.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static ssize_t
read_exact (int fd, void *buf, size_t count)
{
  char *p = buf;
  ssize_t total = 0;

  while ((size_t)total < count)
    {
      ssize_t n = read (fd, p + total, count - total);
      if (n <= 0)
        {
          return (n == 0 && total > 0) ? total : n;
        }
      total += n;
    }

  return total;
}

static ssize_t
write_exact (int fd, const void *buf, size_t count)
{
  const char *p = buf;
  ssize_t total = 0;

  while ((size_t)total < count)
    {
      ssize_t n = write (fd, p + total, count - total);
      if (n <= 0)
        {
          return (n == 0 && total > 0) ? total : n;
        }
      total += n;
    }

  return total;
}

err_t
client_connect (struct client *dest, const char *host, u16 port, error *e)
{
  int fd = socket (AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    {
      return error_causef (e, ERR_IO, "socket: %s", strerror (errno));
    }

  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons (port),
  };

  if (inet_pton (AF_INET, host, &addr.sin_addr) != 1)
    {
      close (fd);
      return error_causef (e, ERR_IO, "inet_pton: bad host '%s'", host);
    }

  if (connect (fd, (struct sockaddr *)&addr, sizeof (addr)) < 0)
    {
      close (fd);
      return error_causef (e, ERR_IO, "connect: %s", strerror (errno));
    }

  dest->fd = fd;
  return SUCCESS;
}

err_t
client_write_all (struct client *c, const void *src, u16 len, error *e)
{
  if (write_exact (c->fd, src, len) != (ssize_t)len)
    {
      return error_causef (e, ERR_IO, "write: %s", strerror (errno));
    }
  return SUCCESS;
}

err_t
client_write_all_size_prefixed (struct client *c, const void *msg, u16 len, error *e)
{
  u8 prefix[4] = {
    (u8) ((len >> 24) & 0xff),
    (u8) ((len >> 16) & 0xff),
    (u8) ((len >> 8) & 0xff),
    (u8) (len & 0xff)
  };

  if (write_exact (c->fd, prefix, 4) != 4)
    {
      return error_causef (e, ERR_IO, "write prefix: %s", strerror (errno));
    }

  if (write_exact (c->fd, msg, len) != (ssize_t)len)
    {
      return error_causef (e, ERR_IO, "write payload: %s", strerror (errno));
    }

  return SUCCESS;
}

err_t
client_read_all (struct client *c, void *dest, u16 len, error *e)
{
  if (read_exact (c->fd, dest, len) != (ssize_t)len)
    {
      return error_causef (e, ERR_IO, "read: %s", strerror (errno));
    }
  return SUCCESS;
}

i32
client_read_all_size_prefixed (struct client *c, void *dest, u16 len, error *e)
{
  u8 prefix[4];

  if (read_exact (c->fd, prefix, 4) != 4)
    {
      return error_causef (e, ERR_IO, "read prefix: %s", strerror (errno));
    }

  u32 msg_len = ((u32)prefix[0] << 24) | ((u32)prefix[1] << 16) | ((u32)prefix[2] << 8) | (u32)prefix[3];

  if (msg_len > len)
    {
      return error_causef (e, ERR_IO, "read: message too large (%u > %u)", msg_len, (u32)len);
    }

  if (read_exact (c->fd, dest, msg_len) != (ssize_t)msg_len)
    {
      return error_causef (e, ERR_IO, "read payload: %s", strerror (errno));
    }

  return msg_len;
}

err_t
client_disconnect (struct client *c, error *e)
{
  if (close (c->fd) < 0)
    {
      return error_causef (e, ERR_IO, "close: %s", strerror (errno));
    }

  c->fd = -1;

  return SUCCESS;
}
