#include <netinet/in.h>
#include <numstore/net/echo_server.h>

static inline u32
decode_prefix (const u8 *p)
{
  return ntohl (*(u32 *)p);
}

static inline void
set_prefix (u32 *dest, u32 src)
{
  src = htonl (src);
  i_memcpy (dest, &src, sizeof (u32));
}

err_t
echo_conn_func (void *echo_ctx, struct connection *conn, error *e)
{
  // Just created a new connection
  if (conn->rx_cap == 0 && conn->tx_cap == 0)
    {
      conn->rx_buf = i_malloc (4, 1, e);
      if (conn->rx_buf == NULL)
        {
          return e->cause_code;
        }
      conn->rx_cap = 4;
      conn->rx_len = 0;
      return SUCCESS;
    }

  // Finished sending tx
  if (conn->tx_cap > 0 && conn->tx_sent == conn->tx_cap)
    {
      conn->tx_cap = 0;
      conn->tx_sent = 0;
      return SUCCESS;
    }

  // Haven't finished recieving yet
  if (conn->rx_len < conn->rx_cap)
    {
      return SUCCESS;
    }

  // Just finished recieving prefix - extend rx buf
  if (conn->rx_cap == 4)
    {
      u32 msg_len = decode_prefix (conn->rx_buf);

      // Empty message (maybe throw error?)
      if (msg_len == 0)
        {
          conn->rx_len = 0;
          return SUCCESS;
        }

      // Allocate recieve buffer
      u32 total = 4 + msg_len;
      void *buf = i_realloc_right (conn->rx_buf, conn->rx_cap, total, 1, e);
      if (buf == NULL)
        {
          return e->cause_code;
        }

      conn->rx_buf = buf;
      conn->rx_cap = total;

      return SUCCESS;
    }

  // Done consuming - do work
  ASSERT (conn->rx_len == conn->rx_cap);
  {
    struct echo_context *ctx = echo_ctx;
    u32 plen = i_strlen (ctx->prefix);
    u32 slen = i_strlen (ctx->suffix);
    u32 mlen = decode_prefix (conn->rx_buf);

    u32 msg_len = mlen + plen + slen;
    u32 frame_len = msg_len + 4;

    conn->tx_buf = i_malloc (frame_len, 1, e);
    if (conn->tx_buf == NULL)
      {
        return e->cause_code;
      }

    // Prefix
    set_prefix (conn->tx_buf, msg_len);

    // Data
    u32 head = 4;
    head += i_memcpy ((u8 *)conn->tx_buf + head, ctx->prefix, plen);
    head += i_memcpy ((u8 *)conn->tx_buf + head, (u8 *)conn->rx_buf + 4, mlen);
    head += i_memcpy ((u8 *)conn->tx_buf + head, ctx->suffix, slen);
    ASSERT (head == frame_len);

    conn->tx_cap = frame_len;
    conn->tx_sent = 0;

    printf ("[>] fd %d  %u bytes\n", conn->fd, frame_len - 4);

    // Reset rx to size 4
    void *buf = i_realloc_right (conn->rx_buf, frame_len - plen - slen, 4, 1, e);
    if (buf == NULL)
      {
        return e->cause_code;
      }

    conn->rx_buf = buf;
    conn->rx_cap = 4;
    conn->rx_len = 0;

    return SUCCESS;
  }
}

struct connection *
echo_conn_alloc (void *ctx, error *e)
{
  struct connection *ret = i_malloc (1, sizeof *ret, e);
  return ret;
}

void
echo_conn_free (void *ctx, struct connection *conn)
{
  i_free (conn);
}
