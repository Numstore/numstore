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
 *   Implements config.h. Provides logging functionality for build and page
 *   configuration parameters including page sizes, memory settings, and layout offsets.
 */

#include <config.h>

#include <numstore/core/system.h>
#include <numstore/intf/logging.h>
#include <numstore/pager/data_list.h>
#include <numstore/pager/inner_node.h>
#include <numstore/pager/page.h>

// core
void
i_log_config (void)
{
  i_log_info ("========== Build / Page Config ==========\n");
  i_log_info ("System Platform: %s\n", platformstr ());

  i_log_info ("PAGE_SIZE        = %" PRIu32 "\n", PAGE_SIZE);
  i_log_info ("MEMORY_PAGE_LEN  = %" PRIu32 "\n", MEMORY_PAGE_LEN);
  i_log_info ("MAX_VSTR         = %" PRIu32 "\n", MAX_VSTR);
  i_log_info ("MAX_TSTR         = %" PRIu32 "\n", MAX_TSTR);
  i_log_info ("TXN_TBL_SIZE     = %" PRIu32 "\n", TXN_TBL_SIZE);
  i_log_info ("WAL_BUFFER_CAP   = %" PRIu32 "\n", WAL_BUFFER_CAP);
  i_log_info ("MAX_NUPD_SIZE    = %" PRIu32 "\n", MAX_NUPD_SIZE);
  i_log_info ("CURSOR_POOL_SIZE = %" PRIu32 "\n", CURSOR_POOL_SIZE);
  i_log_info ("CLI_MAX_FILTERS  = %" PRIu32 "\n", CLI_MAX_FILTERS);

  i_log_info ("-- Page Types --\n");
  i_log_info ("PG_DATA_LIST     = %" PRIu32 "\n", PG_DATA_LIST);
  i_log_info ("PG_INNER_NODE    = %" PRIu32 "\n", PG_INNER_NODE);
  i_log_info ("PG_TOMBSTONE     = %" PRIu32 "\n", PG_TOMBSTONE);
  i_log_info ("PG_ROOT_NODE     = %" PRIu32 "\n", PG_ROOT_NODE);
  i_log_info ("PG_ANY           = %" PRIu32 "\n", PG_ANY);

  // Common page layout
  i_log_info ("-- Common Page Layout --\n");
  i_log_info ("PG_CKSM_OFST     = %" PRIu32 "\n", PG_CKSM_OFST);
  i_log_info ("PG_HEDR_OFST     = %" PRIu32 "\n", PG_HEDR_OFST);
  i_log_info ("PG_PLSN_OFST     = %" PRIu32 "\n", PG_PLSN_OFST);
  i_log_info ("PG_COMMN_END     = %" PRIu32 "\n", PG_COMMN_END);
  i_log_info ("PG_DATA_SIZE     = %" PRIu32 "\n", PG_DATA_SIZE);

  // Data List
  i_log_info ("-- Data List (DL) --\n");
  i_log_info ("DL_NEXT_OFST     = %" PRIu32 "\n", DL_NEXT_OFST);
  i_log_info ("DL_PREV_OFST     = %" PRIu32 "\n", DL_PREV_OFST);
  i_log_info ("DL_BLEN_OFST     = %" PRIu32 "\n", DL_BLEN_OFST);
  i_log_info ("DL_DATA_OFST     = %" PRIu32 "\n", DL_DATA_OFST);
  i_log_info ("DL_DATA_SIZE     = %" PRIu32 "\n", DL_DATA_SIZE);
  i_log_info ("DL_REM           = %" PRIu32 "\n", DL_REM);

  // Inner Node
  i_log_info ("-- Inner Node (IN) --\n");
  i_log_info ("IN_NEXT_OFST     = %" PRIu32 "\n", IN_NEXT_OFST);
  i_log_info ("IN_PREV_OFST     = %" PRIu32 "\n", IN_PREV_OFST);
  i_log_info ("IN_NLEN_OFST     = %" PRIu32 "\n", IN_NLEN_OFST);
  i_log_info ("IN_LEAF_OFST     = %" PRIu32 "\n", IN_LEAF_OFST);
  i_log_info ("IN_MAX_KEYS      = %" PRIu32 "\n", IN_MAX_KEYS);
  i_log_info ("IN_MIN_KEYS      = %" PRIu32 "\n", IN_MIN_KEYS);

  // Variable Page - Removed during reorganization
  // i_log_info ("-- Variable Page (VP) --\n");
  // i_log_info ("VP_NEXT_OFST = %" PRIu32 "\n", VP_NEXT_OFST);
  // i_log_info ("VP_OVNX_OFST = %" PRIu32 "\n", VP_OVNX_OFST);
  // i_log_info ("VP_VLEN_OFST = %" PRIu32 "\n", VP_VLEN_OFST);
  // i_log_info ("VP_TLEN_OFST = %" PRIu32 "\n", VP_TLEN_OFST);
  // i_log_info ("VP_ROOT_OFST = %" PRIu32 "\n", VP_ROOT_OFST);
  // i_log_info ("VP_VNME_OFST = %" PRIu32 "\n", VP_VNME_OFST);
  // i_log_info ("VP_MAX_LEN = %" PRIu32 "\n", VP_MAX_LEN);

  // Variable TAIL - Removed during reorganization
  // i_log_info ("-- Variable Tail (VT) --\n");
  // i_log_info ("VT_NEXT_OFST = %" PRIu32 "\n", VT_NEXT_OFST);
  // i_log_info ("VT_DATA_OFST = %" PRIu32 "\n", VT_DATA_OFST);
  // i_log_info ("VT_DATA_LEN = %" PRIu32 "\n", VT_DATA_LEN);

  i_log_info ("=========================================\n");
}
