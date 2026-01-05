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
 *   Implements threadpool.h. Thread pool with work queue for parallel task execution.
 */

#include <numstore/core/threadpool.h>

#include <numstore/core/assert.h>
#include <numstore/core/closure.h>
#include <numstore/core/error.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/intf/stdlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * 1. ADDING WORK (tp_add_task):
 *
 *    lock(lock)
 *
 *    while (queue_full)
 *        wait(work_ready, lock)     // Block if no space
 *
 *    add_work_to_queue()
 *    broadcast(work_ready)                // Wake workers
 *    unlock(lock)
 *
 * 2. WORKER THREAD (worker_thread):
 *
 *    while (true)
 *        lock(lock)
 *
 *        while (none_working && !stopped)
 *            wait(work_ready, lock) // Block until work available
 *
 *        if (stopped && none_working)
 *            break                        // Exit thread
 *
 *        get_work_from_queue()
 *        broadcast(work_ready)             // Signal space available
 *        nworking++
 *        unlock(lock)
 *
 *        execute_work()
 *
 *        lock(lock)
 *        nworking--
 *
 *        if (none_working && none_workinging)
 *            signal(none_working)         // All work complete
 *
 *        unlock(lock)
 *
 *    nthreads--
 *    signal(none_working)                 // Thread exiting
 *    unlock(lock)
 *
 * 3. STOPPING (tp_stopped):
 *
 *    lock(lock)
 *    stopped = true
 *    broadcast(work_ready)                 // Wake all workers
 *    unlock(lock)
 *
 *    lock(lock)
 *
 *    while (nthreads > 0)
 *        wait(none_working, lock)   // Wait for all threads to exit
 *
 *    unlock(lock)
 */

////////////////////////////////////
/// Worker Thread

static void *
worker_thread (void *arg)
{
  ASSERT (arg);

  struct thread_pool *w = arg;
  struct closure task;

  while (true)
    {
      // Lock resources
      i_mutex_lock (&w->lock);

      /**
       * Wait for work or stopped signal
       *
       * This is in a while loop to handle spurious
       * wakeups
       */
      while (w->count == 0 && !w->stopped)
        {
          i_cond_wait (&w->work_ready, &w->lock);
        }

      /**
       * Done with work
       */
      if (w->stopped && w->count == 0)
        {
          break;
        }

      /**
       * If we got here but there's no work, continue (spurious wakeup)
       */
      if (w->count == 0)
        {
          i_mutex_unlock (&w->lock);
          continue;
        }

      // Get work from queue
      task = w->work_queue[w->tail].task;
      w->work_queue[w->tail].valid = false;
      w->tail = (w->tail + 1) % WORK_QUEUE_SIZE;
      w->count--;

      // Signal that space is available in queue (for blocking add_task)
      i_cond_broadcast (&w->work_ready);

      // Increment working count
      w->nworking++;

      i_mutex_unlock (&w->lock);

      // Execute the work (outside the lock)
      closure_execute (&task);

      // Decrement working count and signal if done
      i_mutex_lock (&w->lock);
      w->nworking--;

      // Signal if all work is complete
      if (!w->stopped && w->nworking == 0 && w->count == 0)
        {
          i_cond_signal (&w->none_working);
        }

      i_mutex_unlock (&w->lock);
    }

  // Thread is exiting
  w->nthreads--;
  i_cond_signal (&w->none_working);
  i_mutex_unlock (&w->lock);

  return NULL;
}

////////////////////////////////////
/// Main lib

struct thread_pool *
tp_open (error *e)
{
  struct thread_pool *w = i_malloc (1, sizeof *w, e);
  if (w == NULL)
    {
      return NULL;
    }

  if (i_mutex_create (&w->lock, e))
    {
      i_free (w);
      return NULL;
    }

  if (i_cond_create (&w->work_ready, NULL))
    {
      i_mutex_free (&w->lock);
      i_free (w);
      return NULL;
    }

  if (i_cond_create (&w->none_working, NULL))
    {
      i_cond_free (&w->work_ready);
      i_mutex_free (&w->lock);
      i_free (w);
      return NULL;
    }

  // Initialize work queue
  i_memset (w->work_queue, 0, sizeof (w->work_queue));
  w->tail = 0;
  w->head = 0;
  w->count = 0;

  w->nworking = 0;
  w->nthreads = 0;
  w->stopped = true;

  return w;
}

err_t
tp_free (struct thread_pool *w, error *e)
{
  if (w == NULL)
    {
      return SUCCESS;
    }

  // Clear any pending work
  i_mutex_lock (&w->lock);
  ASSERT (w->stopped);
  w->count = 0;
  w->tail = 0;
  w->head = 0;

  // Signal threads to stopped
  w->stopped = true;
  i_cond_broadcast (&w->work_ready);
  i_mutex_unlock (&w->lock);

  /**
   * Wait for all threads to finish (handled by tp_stopped if threads are running)
   * If threads aren't running, nthreads will be 0 and this will exit immediately
   */
  i_mutex_lock (&w->lock);
  while (w->nthreads > 0)
    {
      i_cond_wait (&w->none_working, &w->lock);
    }
  i_mutex_unlock (&w->lock);

  // Free synchronization primitives
  i_cond_free (&w->none_working);
  i_cond_free (&w->work_ready);
  i_mutex_free (&w->lock);

  i_free (w);

  return SUCCESS;
}

bool
tp_is_spinning (struct thread_pool *w)
{
  return (w->nthreads > 0);
}

bool
tp_not_spinning (struct thread_pool *w)
{
  return (w->nthreads == 0);
}

err_t
tp_add_task (struct thread_pool *w, void (*func) (void *), void *cl, error *e)
{
  ASSERT (w != NULL);
  ASSERT (func != NULL);

  i_mutex_lock (&w->lock);

  // Block while queue is full
  while (w->count >= WORK_QUEUE_SIZE)
    {
      i_cond_wait (&w->work_ready, &w->lock);
    }

  // Add work to queue
  w->work_queue[w->head].task = (struct closure){
    .func = func,
    .context = cl,
  };
  w->work_queue[w->head].valid = true;
  w->head = (w->head + 1) % WORK_QUEUE_SIZE;
  w->count++;

  // Signal that work is available
  i_cond_broadcast (&w->work_ready);

  i_mutex_unlock (&w->lock);

  return SUCCESS;
}

err_t
tp_execute_until_done (struct thread_pool *w, u32 num_threads, error *e)
{
  tp_spin (w, num_threads, e);
  tp_stop (w, e);
  return e->cause_code;
}

err_t
tp_spin (struct thread_pool *w, u32 num_threads, error *e)
{
  ASSERT (tp_not_spinning (w));
  ASSERT (num_threads >= 1);
  ASSERT (num_threads < arrlen (w->threads));

  i_mutex_lock (&w->lock);
  w->stopped = false;
  w->nthreads = num_threads;
  i_mutex_unlock (&w->lock);

  i_memset (w->threads, 0, sizeof (w->threads));

  for (u32 i = 0; i < num_threads; ++i)
    {
      if (i_thread_create (&w->threads[i], worker_thread, w, e))
        {
          goto failed;
        }
    }

  return SUCCESS;

failed:
  i_log_error ("Failed to create worker threads");

  // Set stopped flag and wake all threads
  i_mutex_lock (&w->lock);
  w->stopped = true;
  i_cond_broadcast (&w->work_ready);
  i_mutex_unlock (&w->lock);

  // Wait for threads to exit
  i_mutex_lock (&w->lock);
  while (w->nthreads > 0)
    {
      i_cond_wait (&w->none_working, &w->lock);
    }
  i_mutex_unlock (&w->lock);

  return e->cause_code;
}

err_t
tp_stop (struct thread_pool *w, error *e)
{
  ASSERT (tp_is_spinning (w));

  i_mutex_lock (&w->lock);

  w->stopped = true;

  // Wake up all waiting threads so they can exit (must hold lock)
  i_cond_broadcast (&w->work_ready);

  i_mutex_unlock (&w->lock);

  u32 threads = w->nthreads;

  // Wait for all threads to finish
  i_mutex_lock (&w->lock);
  while (w->nthreads > 0)
    {
      i_cond_wait (&w->none_working, &w->lock);
    }
  i_mutex_unlock (&w->lock);

  for (u32 i = 0; i < threads; ++i)
    {
      i_thread_join (&w->threads[i], NULL);
    }

  return SUCCESS;
}
