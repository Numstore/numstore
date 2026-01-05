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
 *   Cross-platform OS abstraction layer for file I/O, timers, memory allocation,
 *   and synchronization primitives (mutexes, spinlocks, rwlocks, threads, condvars).
 */

#include <numstore/intf/os/file_system.h>
#include <numstore/intf/os/memory.h>
#include <numstore/intf/os/threading.h>
#include <numstore/intf/os/time.h>

////////////////////////////////////////////////////////////
// Runtime
void i_print_stack_trace (void);
