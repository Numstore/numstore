/*
 * Copyright 2025 Theo Lincke
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 *   Darwin thread operations implementation
 */

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>

#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

////////////////// Thread

err_t
i_thread_create (i_thread *dest,
                 void *(*func) (void *),
                 void *context,
                 error *e)
{
  ASSERT (dest);

#ifndef NDEBUG
  pthread_attr_t attr;
  int r1 = pthread_attr_init (&attr);
  ASSERT (!r1);

  // Examples:
  // pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  // pthread_attr_setstacksize(&attr, 1 << 20);
  // pthread_attr_setguardsize(&attr, 4096);
  // pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  // pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
  // pthread_attr_setschedparam(&attr, &sched_param);

  int r2 = pthread_create (&dest->t, &attr, func, context);

  r1 = pthread_attr_destroy (&attr);
  ASSERT (!r1);
  if (r2)
#else
  if (pthread_create (&dest->t, NULL, func, context))
#endif
    {
      switch (errno)
        {
        case EAGAIN:
          {
            return error_causef (e, ERR_IO, "Failed to create thread: %s", strerror (errno));
          }
        case EINVAL:
          {
            i_log_error ("thread create: invalid attributes: %s\n", strerror (errno));
            UNREACHABLE ();
          }
        case EPERM:
          {
            i_log_error ("thread create: insufficient permissions: %s\n", strerror (errno));
            UNREACHABLE ();
          }
        default:
          {
            UNREACHABLE ();
          }
        }
    }

  return SUCCESS;
}

void
i_thread_join (i_thread *t, void **retval)
{
  ASSERT (t);

  int r = pthread_join (t->t, retval);

  if (r != 0)
    {
      switch (r)
        {
        case EDEADLK:
          {
            i_log_error ("thread join: deadlock detected (joining self?) %s\n",
                         strerror (r));
            UNREACHABLE ();
          }
        case EINVAL:
          {
            i_log_error ("thread join: thread not joinable %s\n",
                         strerror (r));
            UNREACHABLE ();
          }
        case ESRCH:
          {
            i_log_error ("thread join: no such thread %s\n",
                         strerror (r));
            UNREACHABLE ();
          }
        default:
          {
            UNREACHABLE ();
          }
        }
    }
}

void
i_thread_cancel (i_thread *t)
{
  ASSERT (t);

  int r = pthread_cancel (t->t);
  if (r != 0)
    {
      switch (r)
        {
        case ESRCH:
          {
            i_log_error ("thread cancel: no such thread %s\n", strerror (r));
            UNREACHABLE ();
          }
        default:
          {
            UNREACHABLE ();
          }
        }
    }
}

u64
get_available_threads (void)
{
  long ret = sysconf (_SC_NPROCESSORS_ONLN);
  ASSERT (ret > 0);
  return (u64)ret;
}
