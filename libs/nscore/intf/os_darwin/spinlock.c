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
 *   Darwin spinlock operations implementation
 */

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>

#include <errno.h>
#include <pthread.h>
#include <string.h>

////////////////// Spin Lock

err_t
i_spinlock_create (i_spinlock *dest, error *e)
{
  ASSERT (dest);
  errno = 0;

  // PTHREAD_PROCESS_PRIVATE means the spinlock is only shared between threads
  // of the same process (not across processes)
  if (pthread_spin_init (&dest->lock, PTHREAD_PROCESS_PRIVATE))
    {
      switch (errno)
        {
        case EAGAIN:
          {
            return error_causef (
                e, ERR_IO,
                "Failed to initialize spinlock: %s",
                strerror (errno));
          }
        case ENOMEM:
          {
            return error_causef (
                e, ERR_NOMEM,
                "Failed to initialize spinlock: %s",
                strerror (errno));
          }
        case EINVAL:
          {
            i_log_error ("spinlock create: invalid pshared value: %s\n",
                         strerror (errno));
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
i_spinlock_free (i_spinlock *m)
{
  ASSERT (m);
  errno = 0;

  if (pthread_spin_destroy (&m->lock))
    {
      switch (errno)
        {
        case EBUSY:
          {
            i_log_error ("Spinlock is locked! %s\n", strerror (errno));
            UNREACHABLE ();
          }
        case EINVAL:
          {
            i_log_error ("Invalid Spinlock! %s\n", strerror (errno));
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
i_spinlock_lock (i_spinlock *m)
{
  ASSERT (m);
  errno = 0;

  if (pthread_spin_lock (&m->lock))
    {
      switch (errno)
        {
        case EINVAL:
          {
            i_log_error ("spinlock lock: Invalid spinlock! %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        case EDEADLK:
          {
            i_log_error ("spinlock lock: Deadlock detected! %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        default:
          {
            i_log_error ("spinlock lock: Unknown error detected! %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        }
    }
}

void
i_spinlock_unlock (i_spinlock *m)
{
  ASSERT (m);
  errno = 0;

  if (pthread_spin_unlock (&m->lock))
    {
      switch (errno)
        {
        case EINVAL:
          {
            i_log_error ("spinlock unlock: Invalid spinlock! %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        case EPERM:
          {
            i_log_error ("spinlock unlock: current thread doesn't own this spinlock: %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        default:
          {
            i_log_error ("spinlock unlock: Unknown error detected! %s\n",
                         strerror (errno));
            UNREACHABLE ();
          }
        }
    }
}
