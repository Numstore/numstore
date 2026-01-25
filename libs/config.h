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
 *   Build configuration header defining compile-time constants for the NumStore system.
 *   Specifies page sizes, memory limits, transaction table sizes, WAL buffer capacity,
 *   cursor pool settings, and other fundamental system parameters. Provides logging
 *   function for configuration inspection.
 */

#include <numstore/intf/types.h>

#define PAGE_POW 12
#define PAGE_SIZE ((p_size)1 << PAGE_POW)
#define MEMORY_PAGE_LEN ((u32)20)
#define MAX_VSTR 10000
#define MAX_TSTR 10000
#define TXN_TBL_SIZE 512
#define WAL_BUFFER_CAP 1000000
#define MAX_NUPD_SIZE 200
#define CURSOR_POOL_SIZE 100
#define CLI_MAX_FILTERS 32
#define MAX_TIDS 1000
#define MAX_OPEN_FILES 10
#define MAX_FILE_NAME 4096
#define WAL_SEGMENT_SIZE (16 * 1024 * 1024) // 16 MB

// Address: [ file type ] [ file number ] [ file offset ]
#define FILE_TYPE_BITS 4
#define FILE_NUM_BITS 36
#define FILE_OFST_BITS 24

// #define DUMB_PAGER

void i_log_config (void);
