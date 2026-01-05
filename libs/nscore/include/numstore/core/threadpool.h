#pragma once

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
 *   Fixed-size thread pool with work queue for parallel task execution, supporting
 *   task scheduling, execution-until-done, and continuous spinning modes.
 */

#include <numstore/core/assert.h>
#include <numstore/core/closure.h>
#include <numstore/core/error.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/intf/stdlib.h>
#include <numstore/intf/types.h>

#define WORK_QUEUE_SIZE 1024

struct work_item
{
  struct closure task;
  bool valid;
};

struct thread_pool
{
  struct work_item work_queue[WORK_QUEUE_SIZE];
  u32 tail;  // Read index
  u32 head;  // Write index
  u32 count; // Number of items in queue

  i_mutex lock;        // Single mutex protects queue and synchronizes condition variables
  i_cond work_ready;   // Signal when tasks are added
  i_cond none_working; // Signal when no threads are working

  i_thread threads[20];

  u32 nworking; // Number of threads actively processing
  u32 nthreads; // Number of threads alive
  bool stopped; // Signal threads to stopped
};

// Lifecycle
struct thread_pool *tp_open (error *e);
err_t tp_free (struct thread_pool *w, error *e);

// Api
bool tp_is_spinning (struct thread_pool *w);
bool tp_not_spinning (struct thread_pool *w);
err_t tp_add_task (struct thread_pool *w, void (*func) (void *), void *context, error *e);
err_t tp_execute_until_done (struct thread_pool *w, u32 num_threads, error *e);
err_t tp_spin (struct thread_pool *w, u32 num_threads, error *e);
err_t tp_stop (struct thread_pool *w, error *e);
