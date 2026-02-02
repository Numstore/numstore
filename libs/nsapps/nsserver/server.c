#include <netinet/in.h>
#include <numstore/apps/server/nsconnection.h>
#include <numstore/apps/server/server.h>
#include <numstore/core/error.h>
#include <numstore/core/macros.h>
#include <numstore/core/slab_alloc.h>

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

struct connection *
nsconn_alloc (void *ctx, error *e)
{
  struct connection_manager *mgr = ctx;

  struct nsconnection *ret = slab_alloc_alloc (&mgr->conn_alloc, e);
  if (ret == NULL)
    {
      return NULL;
    }

  return &ret->conn;
}

err_t
nsconn_func (void *ctx, struct connection *_conn, error *e)
{
  struct connection_manager *mgr = ctx;

  struct nsconnection *conn = container_of (_conn, struct nsconnection, conn);

  switch (conn->state)
    {
    case READING_COMMAND_PREFIX:
      {
      }
    case READING_COMMAND:
      {
      }
    case EXECUTING_COMMAND:
      {
      }
    case WRITING_RESULT:
      {
      }
    }

  return SUCCESS;
}

void
nsconn_free (void *ctx, struct connection *_conn, error *e)
{
  struct connection_manager *mgr = ctx;

  struct nsconnection *conn = container_of (_conn, struct nsconnection, conn);

  slab_alloc_free (&mgr->conn_alloc, conn);
}
