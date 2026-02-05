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
 *   Fast, lightweight blocking SPSC (Single-Producer Single-Consumer)
 *   circular buffer. Uses atomics for lock-free fast path and futex
 *   for efficient blocking when buffer is empty/full.
 *
 * Design:
 *   - Producer owns 'head', consumer owns 'tail'
 *   - No locks on fast path - just atomic load/store with proper ordering
 *   - Futex-based blocking only when actually waiting (empty/full)
 *   - Cache-line padding to prevent false sharing
 */

#ifndef SPSC_CBUFFER_H
#define SPSC_CBUFFER_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t i32;

/* Cache line size for padding - prevents false sharing */
#define CACHE_LINE_SIZE 64

/*
 * Blocking SPSC circular buffer.
 *
 * Memory layout optimized to keep producer and consumer state
 * on separate cache lines to avoid false sharing.
 */
struct spsc_cbuffer
{
  // Producer
  _Alignas(CACHE_LINE_SIZE) _Atomic u32 head; // write position
  u32 cached_tail;                            // producer's cached copy of tail (reduces atomic reads)

  // Consumer
  _Alignas(CACHE_LINE_SIZE) _Atomic u32 tail; // read position
  u32 cached_head;                            // consumer's cached copy of head (reduces atomic reads)

  // Shared (read only)
  _Alignas(CACHE_LINE_SIZE) u32 cap; // capacity in bytes
  u32 mask;                          // cap - 1, for fast modulo (requires power-of-2 cap)
  u8 *data;                          // buffer storage

  /* === Blocking state === */
  _Alignas(CACHE_LINE_SIZE) _Atomic u32 waiters; /* bitmask: bit 0 = producer waiting, bit 1 = consumer waiting */
};

#define SPSC_PRODUCER_WAITING (1u << 0)
#define SPSC_CONSUMER_WAITING (1u << 1)

/*
 * Initialize an SPSC buffer.
 *
 * @param data   Pre-allocated buffer storage
 * @param cap    Capacity in bytes (MUST be power of 2)
 * @return       Initialized spsc_cbuffer
 */
struct spsc_cbuffer spsc_cbuffer_create (void *data, u32 cap);

/*
 * Convenience macro: create from a stack array.
 * Array size must be power of 2.
 */
#define spsc_cbuffer_create_from(arr) spsc_cbuffer_create ((arr), sizeof (arr))

/*
 * Get available space for writing (producer side).
 * Lock-free, uses cached tail.
 */
static inline u32
spsc_avail (struct spsc_cbuffer *b)
{
  u32 head = atomic_load_explicit (&b->head, memory_order_relaxed);
  return b->cap - (head - b->cached_tail);
}

/*
 * Get available data for reading (consumer side).
 * Lock-free, uses cached head.
 */
static inline u32
spsc_len (struct spsc_cbuffer *b)
{
  u32 tail = atomic_load_explicit (&b->tail, memory_order_relaxed);
  return b->cached_head - tail;
}

/*
 * Check if buffer is empty (consumer perspective).
 */
static inline bool
spsc_isempty (struct spsc_cbuffer *b)
{
  return spsc_len (b) == 0;
}

/*
 * Check if buffer is full (producer perspective).
 */
static inline bool
spsc_isfull (struct spsc_cbuffer *b)
{
  return spsc_avail (b) == 0;
}

/* ============================================================
 * NON-BLOCKING OPERATIONS (return false if would block)
 * ============================================================ */

/*
 * Try to write data without blocking.
 *
 * @param src    Source data
 * @param len    Number of bytes to write
 * @param b      Buffer
 * @return       Number of bytes written (may be less than len)
 */
u32 spsc_try_write (const void *src, u32 len, struct spsc_cbuffer *b);

/*
 * Try to read data without blocking.
 *
 * @param dest   Destination buffer
 * @param len    Number of bytes to read
 * @param b      Buffer
 * @return       Number of bytes read (may be less than len)
 */
u32 spsc_try_read (void *dest, u32 len, struct spsc_cbuffer *b);

/* ============================================================
 * BLOCKING OPERATIONS (wait until complete)
 * ============================================================ */

/*
 * Write data, blocking until all bytes are written.
 * Only call from producer thread.
 *
 * @param src    Source data
 * @param len    Number of bytes to write
 * @param b      Buffer
 */
void spsc_write_blocking (const void *src, u32 len, struct spsc_cbuffer *b);

/*
 * Read data, blocking until all bytes are read.
 * Only call from consumer thread.
 *
 * @param dest   Destination buffer
 * @param len    Number of bytes to read
 * @param b      Buffer
 */
void spsc_read_blocking (void *dest, u32 len, struct spsc_cbuffer *b);

/*
 * Write exactly len bytes, blocking if necessary.
 * Returns only when all bytes written.
 */
void spsc_write_exact (const void *src, u32 len, struct spsc_cbuffer *b);

/*
 * Read exactly len bytes, blocking if necessary.
 * Returns only when all bytes read.
 */
void spsc_read_exact (void *dest, u32 len, struct spsc_cbuffer *b);

/* ============================================================
 * SINGLE ELEMENT OPERATIONS
 * ============================================================ */

/*
 * Push a single element (blocking).
 */
static inline void
spsc_push (const void *src, u32 size, struct spsc_cbuffer *b)
{
  spsc_write_exact (src, size, b);
}

/*
 * Pop a single element (blocking).
 */
static inline void
spsc_pop (void *dest, u32 size, struct spsc_cbuffer *b)
{
  spsc_read_exact (dest, size, b);
}

/*
 * Try push without blocking.
 * @return true if pushed, false if buffer full
 */
bool spsc_try_push (const void *src, u32 size, struct spsc_cbuffer *b);

/*
 * Try pop without blocking.
 * @return true if popped, false if buffer empty
 */
bool spsc_try_pop (void *dest, u32 size, struct spsc_cbuffer *b);

/* ============================================================
 * SHUTDOWN / WAKEUP
 * ============================================================ */

/*
 * Wake up any waiting threads (useful for shutdown).
 */
void spsc_wake_all (struct spsc_cbuffer *b);

#endif /* SPSC_CBUFFER_H */
