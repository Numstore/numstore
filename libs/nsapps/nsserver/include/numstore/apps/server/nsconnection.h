#include "numstore/core/chunk_alloc.h"
#include <numstore/net/polling_server.h>

struct nsconnection
{
  enum
  {
    READING_COMMAND_PREFIX,
    READING_COMMAND,
    EXECUTING_COMMAND,
    WRITING_RESULT,
  } state;

  u16 prefix;
  struct chunk_alloc alloc;

  struct connection conn;
};

err_t nsconn_execute_read_command_prefix (struct nsconnection *conn, error *e);
err_t nsconn_execute_read_command (struct nsconnection *conn, error *e);
