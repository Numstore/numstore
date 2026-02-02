

#include "numstore/core/error.h"
#include "numstore/core/threadpool.h"
#include "numstore/intf/os/memory.h"
#include "numstore/intf/os/threading.h"
#include "numstore/net/client.h"
#include "numstore/net/echo_server.h"
#include "numstore/net/polling_server.h"

static void
server_thread (void *args)
{
  struct polling_server *server = args;
  error e = error_create ();

  while (pserv_execute (server, &e) > 0)
    {
    }
}

static void
client_thread (void *args)
{
  error e = error_create ();

  struct client c;
  if (client_connect (&c, "127.0.0.1", 8080, &e))
    {
      return;
    }

  char buffer[2048];

  for (u32 i = 0; i < 1000; ++i)
    {
      if (client_write_all_size_prefixed (&c, "Hello World", strlen ("Hello world"), &e))
        {
          break;
        }

      i32 n = client_read_all_size_prefixed (&c, buffer, arrlen (buffer), &e);
      if (n < 0)
        {
          break;
        }

      fprintf (stdout, "%d %.*s\n", i, n, buffer);
    }
}

int
main (void)
{
  error e = error_create ();
  struct thread_pool *tp = tp_open (&e);
  if (tp == NULL)
    {
      return e.cause_code;
    }

  if (tp_spin (tp, get_available_threads (), &e))
    {
      return e.cause_code;
    }

  struct echo_context ctx = {
    .prefix = "PREFIX",
    .suffix = "SUFFIX",
  };

  struct polling_server server;
  if (pserv_open (&server, 8080,
                  (struct conn_actions){
                      .conn_alloc = echo_conn_alloc,
                      .conn_func = echo_conn_func,
                      .conn_free = echo_conn_free,
                  },
                  &ctx, &e))
    {
      return e.cause_code;
    }

  if (tp_add_task (tp, server_thread, &server, &e))
    {
      return e.cause_code;
    }

  for (u32 i = 0; i < 100; ++i)
    {
      if (tp_add_task (tp, client_thread, NULL, &e))
        {
          return e.cause_code;
        }
    }

  if (tp_stop (tp, &e))
    {
      return e.cause_code;
    }

  return 0;
}
