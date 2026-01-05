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
 *   Write-ahead log record header structure defining log entry types and metadata for recovery operations.
 */

#include <numstore/intf/types.h>
#include <numstore/pager/dirty_page_table.h>
#include <numstore/pager/txn_table.h>

#include <config.h>

struct wal_update_read
{
  txid tid;
  lsn prev;
  pgno pg;
  u8 undo[PAGE_SIZE];
  u8 redo[PAGE_SIZE];
};

struct wal_update_write
{
  txid tid;
  lsn prev;
  pgno pg;
  u8 *redo;
  u8 *undo;
};

struct wal_begin
{
  txid tid;
};

struct wal_commit
{
  txid tid;
  lsn prev;
};

struct wal_end
{
  txid tid;
  lsn prev;
};

struct wal_clr_read
{
  txid tid;
  lsn prev;
  pgno pg;
  lsn undo_next;
  u8 redo[PAGE_SIZE];
};

struct wal_clr_write
{
  txid tid;
  lsn prev;
  pgno pg;
  lsn undo_next;
  u8 *redo;
  u8 read_redo[PAGE_SIZE];
};

struct wal_ckpt_end_read
{
  struct txn_table att;
  struct dpg_table *dpt;
  struct txn *txn_bank;
};

struct wal_ckpt_end_write
{
  struct txn_table *att;
  struct dpg_table *dpt;
};

enum wal_rec_hdr_type
{
  WL_BEGIN = 1,
  WL_COMMIT = 2,
  WL_END = 3,
  WL_UPDATE = 4,
  WL_CLR = 5,
  WL_CKPT_BEGIN = 6,
  WL_CKPT_END = 7,
  WL_EOF = 8,
};

struct wal_rec_hdr_read
{
  enum wal_rec_hdr_type type;

  union
  {
    struct wal_update_read update;
    struct wal_begin begin;
    struct wal_commit commit;
    struct wal_end end;
    struct wal_clr_read clr;
    struct wal_ckpt_end_read ckpt_end;
  };
};

struct wal_rec_hdr_write
{
  enum wal_rec_hdr_type type;

  union
  {
    struct wal_update_write update;
    struct wal_begin begin;
    struct wal_commit commit;
    struct wal_end end;
    struct wal_clr_write clr;
    struct wal_ckpt_end_write ckpt_end;
  };
};

const char *wal_rec_hdr_type_tostr (enum wal_rec_hdr_type type);

struct wal_rec_hdr_write wrhw_from_wrhr (struct wal_rec_hdr_read *src);

// BEGIN (only tid, no prev)
#define WL_BEGIN_LEN (sizeof (wlh)    /* header */         \
                      + sizeof (txid) /* Transaction id */ \
                      + sizeof (u32)) /* Checksum */

#define WL_COMMIT_LEN (sizeof (wlh)    /* Header */         \
                       + sizeof (txid) /* transaction id */ \
                       + sizeof (lsn)  /* prev lsn */       \
                       + sizeof (u32)) /* Checksum */

#define WL_END_LEN (sizeof (wlh)    /* header */         \
                    + sizeof (txid) /* transaction id */ \
                    + sizeof (lsn)  /* last lsn */       \
                    + sizeof (u32)) /* Checksum */

// Size of Update Entry
#define WL_UPDATE_LEN (sizeof (wlh)    /* header */         \
                       + sizeof (txid) /* transaction id */ \
                       + sizeof (lsn)  /* prev lsn */       \
                       + sizeof (pgno) /* ref page */       \
                       + PAGE_SIZE     /* Undo */           \
                       + PAGE_SIZE     /* Redo */           \
                       + sizeof (u32)) /* Checksum */

// Size of CLR entry
#define WL_CLR_LEN (sizeof (wlh)    /* header */          \
                    + sizeof (txid) /* transaction id */  \
                    + sizeof (lsn)  /* prev lsn */        \
                    + sizeof (pgno) /* ref page */        \
                    + sizeof (lsn)  /* update next lsn */ \
                    + PAGE_SIZE     /* Data */            \
                    + sizeof (u32)) /* Checksum */

#define WL_CKPT_BEGIN_LEN (sizeof (wlh)    /* Header */ \
                           + sizeof (u32)) /* Checksum */

#define WL_CKPT_END_MAX_LEN (sizeof (wlh)        /* header */ \
                             + MAX_TXNT_SRL_SIZE /* att */    \
                             + MAX_DPGT_SRL_SIZE /* dptgt */  \
                             + sizeof (u32))     /* Checksum */

stxid wrh_get_tid (struct wal_rec_hdr_read *h);
slsn wrh_get_prev_lsn (struct wal_rec_hdr_read *h);
void i_log_wal_rec_hdr_read (int log_level, struct wal_rec_hdr_read *r);
void i_print_wal_rec_hdr_read_light (int log_level, const struct wal_rec_hdr_read *w, lsn l);

// DECODE
void walf_decode_update (struct wal_rec_hdr_read *r, const u8 buf[WL_UPDATE_LEN]);
void walf_decode_clr (struct wal_rec_hdr_read *r, const u8 buf[WL_CLR_LEN]);
void walf_decode_begin (struct wal_rec_hdr_read *r, const u8 buf[WL_BEGIN_LEN]);
void walf_decode_commit (struct wal_rec_hdr_read *r, const u8 buf[WL_COMMIT_LEN]);
void walf_decode_end (struct wal_rec_hdr_read *r, const u8 buf[WL_END_LEN]);
void walf_decode_ckpt_begin (struct wal_rec_hdr_read *r, const u8 buf[WL_CKPT_BEGIN_LEN]);
err_t walf_decode_ckpt_end (struct wal_rec_hdr_read *r, const u8 *buf, error *e);

#ifndef NTEST
bool wal_rec_hdr_read_equal (struct wal_rec_hdr_read *left, struct wal_rec_hdr_read *right);
#endif
