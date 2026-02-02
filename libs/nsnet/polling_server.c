#include "numstore/core/latch.h"
#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/core/slab_alloc.h>
#include <numstore/intf/os.h>
#include <numstore/net/polling_server.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

err_t
pserv_open (struct polling_server *ps, int port, struct conn_actions actions, void *ctx, error *e)
{
  ps->actions = actions;
  ps->ctx = ctx;
  ps->cap = 100 + 1;
  ps->fds = i_malloc (ps->cap, sizeof *ps->fds, e);
  if (ps->fds == NULL)
    {
      return e->cause_code;
    }

  ps->conns = i_malloc (ps->cap, sizeof (struct connection *), e);
  if (ps->conns == NULL)
    {
      i_free (ps->fds);
      return e->cause_code;
    }
  ps->conns[0] = NULL;

  int sfd = socket (AF_INET, SOCK_STREAM, 0);
  if (sfd < 0)
    {
      i_free (ps->conns);
      i_free (ps->fds);
      return error_causef (e, ERR_IO, "socket: %s", strerror (errno));
    }

  int opt = 1;
  if (setsockopt (sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt)) < 0)
    {
      close (sfd);
      i_free (ps->conns);
      i_free (ps->fds);
      return error_causef (e, ERR_IO, "setsockopt: %s", strerror (errno));
    }

  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons (port),
    .sin_addr = { .s_addr = INADDR_ANY }
  };

  if (bind (sfd, (struct sockaddr *)&addr, sizeof (addr)) < 0)
    {
      close (sfd);
      i_free (ps->conns);
      i_free (ps->fds);
      return error_causef (e, ERR_IO, "bind: %s", strerror (errno));
    }

  if (listen (sfd, SOMAXCONN) < 0)
    {
      close (sfd);
      i_free (ps->conns);
      i_free (ps->fds);
      return error_causef (e, ERR_IO, "listen: %s", strerror (errno));
    }

  ps->fds[0].fd = sfd;
  ps->fds[0].events = POLLIN;
  ps->len = 1;
  ps->running = 1;

  return SUCCESS;
}

static err_t
grow_if_needed (struct polling_server *s, error *e)
{
  if (s->len < s->cap)
    {
      return SUCCESS;
    }

  u32 new_cap = s->cap * 2;

  struct pollfd *fds = i_realloc_right (s->fds, s->cap, new_cap, sizeof (struct pollfd), e);
  if (fds == NULL)
    {
      return e->cause_code;
    }
  s->fds = fds;

  struct connection **conns = i_realloc_right (s->conns, s->cap, new_cap, sizeof (struct connection *), e);
  if (conns == NULL)
    {
      return e->cause_code;
    }
  s->conns = conns;
  s->cap = new_cap;

  return SUCCESS;
}

static inline err_t
pserv_execute_conns (struct polling_server *s, error *e)
{
  for (u32 i = 1; i < s->len; ++i)
    {
      struct connection *conn = s->conns[i];
      ASSERT (conn);

      if (s->actions.conn_func (s->ctx, conn, e))
        {
          return e->cause_code;
        }

      s->fds[i].events = 0;

      latch_lock (&conn->l);

      bool reading = conn->rx_cap > 0;
      bool writing = conn->tx_cap > 0;

      bool done_reading = conn->rx_len == conn->rx_cap;
      bool done_writing = conn->tx_sent == conn->tx_cap;

      latch_unlock (&conn->l);

      if (reading && !done_reading)
        {
          s->fds[i].events |= POLLIN;
        }

      if (writing && !done_writing)
        {
          s->fds[i].events |= POLLOUT;
        }
    }

  return SUCCESS;
}

static err_t
handle_accept (struct polling_server *s, error *e)
{
  // Allocate new connection
  struct connection *conn = s->actions.conn_alloc (s->ctx, e);
  if (conn == NULL)
    {
      return e->cause_code;
    }

  i_memset (conn, 0, sizeof *conn);

  latch_init (&conn->l);

  struct sockaddr_in peer;
  socklen_t peer_len = sizeof (peer);

  conn->fd = accept (s->fds[0].fd, (struct sockaddr *)&peer, &peer_len);
  if (conn->fd < 0)
    {
      s->actions.conn_free (s->ctx, conn);
      return error_causef (e, ERR_IO, "accept: %s", strerror (errno));
    }

  if (grow_if_needed (s, e))
    {
      close (conn->fd);
      s->actions.conn_free (s->ctx, conn);
      return e->cause_code;
    }

  s->fds[s->len].fd = conn->fd;
  s->fds[s->len].events = 0;
  s->conns[s->len] = conn;
  s->len++;

  printf ("[+] connected %s:%d  (fd %d, %d clients)\n",
          inet_ntoa (peer.sin_addr), ntohs (peer.sin_port),
          conn->fd, s->len - 1);

  return SUCCESS;
}

static err_t
remove_client (struct polling_server *s, u32 i, error *e)
{
  struct connection *conn = s->conns[i];

  latch_lock (&conn->l);

  if (close (s->fds[i].fd) < 0)
    {
      return error_causef (e, ERR_IO, "close: %s", strerror (errno));
    }

  i_free (conn->rx_buf);
  i_free (conn->tx_buf);

  // TODO - conn use after free
  latch_unlock (&conn->l);

  s->actions.conn_free (s->ctx, conn);

  /* Swap-remove from both arrays. */
  s->fds[i] = s->fds[s->len - 1];
  s->conns[i] = s->conns[s->len - 1];
  s->len--;

  return SUCCESS;
}

static err_t
handle_read (struct connection *conn, error *e)
{
  latch_lock (&conn->l);

  ASSERT (conn->rx_cap > conn->rx_len);
  ssize_t n = read (conn->fd, (char *)conn->rx_buf + conn->rx_len, conn->rx_cap - conn->rx_len);

  if (n < 0)
    {
      error_causef (e, ERR_IO, "read: %s", strerror (errno));
      goto theend;
    }

  if (n == 0)
    {
      printf ("[-] fd %d closed\n", conn->fd);
      error_causef (e, ERR_IO, "read: peer closed connection");
      goto theend;
    }

  conn->rx_len += n;

theend:
  latch_unlock (&conn->l);

  return e->cause_code;
}

static err_t
handle_write (struct connection *conn, error *e)
{
  latch_lock (&conn->l);

  ASSERT (conn->tx_cap > conn->tx_sent);
  ssize_t n = write (conn->fd, (char *)conn->tx_buf + conn->tx_sent, conn->tx_cap - conn->tx_sent);

  if (n < 0)
    {
      error_causef (e, ERR_IO, "write: %s", strerror (errno));
      goto theend;
    }

  conn->tx_sent += n;

theend:
  latch_unlock (&conn->l);
  return SUCCESS;
}

int
pserv_execute (struct polling_server *ps, error *e)
{
  // Execute connections and set polling events
  if (pserv_execute_conns (ps, e))
    {
      return e->cause_code;
    }

  int ret = poll (ps->fds, ps->len, 1000);

  if (ret < 0)
    {
      if (errno == EINTR)
        {
          return ps->running;
        }
      return error_causef (e, ERR_IO, "poll: %s", strerror (errno));
    }

  if (ret == 0)
    {
      return ps->running;
    }

  /* Walk in reverse so swap-remove doesn't skip entries. */
  for (int i = ps->len - 1; i >= 0; i--)
    {
      // Check for events
      int ev = ps->fds[i].revents;
      if (!ev)
        {
          continue;
        }

      // Server only accepts
      if (i == 0)
        {
          ASSERT (ev & POLLIN);
          if (handle_accept (ps, e))
            {
              return e->cause_code;
            }
        }

      // Error
      else if (ev & (POLLERR | POLLHUP))
        {
          printf ("[-] fd %d hangup/error (0x%x)\n", ps->fds[i].fd, ev);
          if (remove_client (ps, i, e))
            {
              return e->cause_code;
            }
        }

      // Regular client operation
      else
        {
          struct connection *conn = ps->conns[i];
          ASSERT (conn);

          if (ev & POLLIN)
            {
              if (handle_read (conn, e))
                {
                  if (remove_client (ps, i, e))
                    {
                      return e->cause_code;
                    }
                }
            }
          else if (ev & POLLOUT)
            {
              if (handle_write (conn, e))
                {
                  if (remove_client (ps, i, e))
                    {
                      return e->cause_code;
                    }
                }
            }
        }
    }

  return ps->running;
}

err_t
pserv_close (struct polling_server *ps, error *e)
{
  for (u32 i = 0; i < ps->len; i++)
    {
      if (close (ps->fds[i].fd) < 0)
        {
          error_causef (e, ERR_IO, "close fd %d: %s", ps->fds[i].fd, strerror (errno));
        }
    }

  /* Free each connection and return it to the slab. */
  for (u32 i = 1; i < ps->len; i++)
    {
      latch_lock (&ps->conns[i]->l);
      i_free (ps->conns[i]->rx_buf);
      i_free (ps->conns[i]->tx_buf);
      // TODO - conn use after free
      latch_unlock (&ps->conns[i]->l);
      ps->actions.conn_free (ps->ctx, ps->conns[i]);
    }

  i_free (ps->conns);
  i_free (ps->fds);

  ps->conns = NULL;
  ps->fds = NULL;
  ps->len = 0;
  ps->cap = 0;

  return e->cause_code;
}
