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
 *   Defines types and interfaces for complex memory copy operations with access patterns.
 *   Supports strictly increasing accessors for monotonic source reads with non-monotonic
 *   destination writes, and jump accessors for non-contiguous source reads with monotonic
 *   destination writes. Provides stateful, resumable memory copy operations.
 */

#include <numstore/intf/types.h>

/**
 * Strictly increasing accessor
 *
 * Dest is fixed length and known - src is on demand - use a hop buffer
 * for source in order to read monotonically from source, but write to non monotonic
 * offsets in dest
 *
 * [_________++--++--++--_________+++++_____+++++]
 * 					 ^
 * 					ofst0				[next_hop]
 * 					 [   acc    ]
 */

/**
 * A contiguous access pattern,
 *
 * Accessing a contiguous array of bytes
 */
struct acc_pat
{
  u32 start;  // Starting byte
  u32 nreads; // Number of reads to execute
  u32 read;   // Number of bytes to read
  u32 skip;   // Number of bytes to skip
};

// Memcpy Strict Increasing State
struct mstist
{
  u32 hnum;        // Hop number we're currently on
  u32 hidx;        // Location within the current hop
  u32 lidx;        // Location within the current access pattern state
  u32 nread;       // Number of elements read in access pattern
  bool isskipping; // Is skipping within the access pattern
};

typedef struct
{
  u32 hop;            // Starting hop before this access pattern
  struct acc_pat acc; // Access Pattern
  u32 dofst;          // Destination offset for each access pattern
} mtipl_entry;

// Memcpy Strict Increasing Plan
struct mtipl
{
  mtipl_entry *accs;
  u32 nacc;
};

bool memcpy_strinc_acc (
    u8 *dest,
    const u8 *src,
    const u32 slen,
    const struct mtipl plan,
    struct mstist *state);

/**
 * Jump around accessor
 *
 * Source is fixed length and known - dest is on demand - use an offset buffer
 * for source and write monotonically increasing to destination
 *
 * [_________++--++--++--++--++--_________+-+-+-+-+-+-+-+-+-_____+++++]
 * 					 ^													^
 * 			ofsts[1].ofst 								ofsts[0].ofst
 * 					 [  ofsts[1].acc    ]				  [  ofsts[0].acc  ]
 */

// Memcpy Jump State
struct mjmst
{
  u32 onum;        // Offset number we're currently on
  u32 lidx;        // Location within the current access pattern state
  u32 nread;       // Number of elements read in access pattern
  bool isskipping; // Is skipping within the access pattern
};

typedef struct
{
  u32 ofst;           // Source offsets for each element
  struct acc_pat acc; // Access Patterns for each offset
} mjmpl_entry;

// Memcpy Jump Plan
struct mjmpl
{
  mjmpl_entry *accs;
  u32 nacc; // Number of access patterns
};

bool memcpy_jmp_acc (
    u8 *dest,                // Destination
    const u32 dlen,          // Length of the destination buffer
    const u8 *src,           // Source array (length == enough to fit the parameters)
    const struct mjmpl plan, // Memcpy Plan
    struct mjmst *state      // State for stop and continue
);
