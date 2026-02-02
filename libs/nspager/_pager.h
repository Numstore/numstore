#include <file_pager.h>

#include <numstore/core/adptv_hash_table.h>
#include <numstore/core/assert.h>
#include <numstore/core/dbl_buffer.h>
#include <numstore/core/error.h>
#include <numstore/core/latch.h>
#include <numstore/core/max_capture.h>
#include <numstore/core/random.h>
#include <numstore/core/string.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>
#include <numstore/intf/types.h>
#include <numstore/pager.h>
#include <numstore/pager/data_list.h>
#include <numstore/pager/dirty_page_table.h>
#include <numstore/pager/lock_table.h>
#include <numstore/pager/lt_lock.h>
#include <numstore/pager/page.h>
#include <numstore/pager/page_h.h>
#include <numstore/pager/root_node.h>
#include <numstore/pager/tombstone.h>
#include <numstore/pager/txn.h>
#include <numstore/pager/txn_table.h>
#include <numstore/pager/wal_file.h>
#include <numstore/pager/wal_stream.h>
#include <numstore/test/page_fixture.h>
#include <numstore/test/testing.h>

#include <config.h>
#include <wal.h>

#define KTYPE pgno
#define VTYPE u32
#define SUFFIX idx
#include <numstore/core/robin_hood_ht.h>
#undef KTYPE
#undef VTYPE
#undef SUFFIX

///////////////////////////////////////////////////////////
////// UTILS

enum
{
  PW_ACCESS = 1u << 0, // Only used for readable
  PW_DIRTY = 1u << 1,  // Only used for readable
  PW_PRESENT = 1u << 2,
  PW_X = 1u << 3,
};

static inline bool
pf_check (const struct page_frame *pf, int flag)
{
  return (pf->flags & flag) != 0;
}

static inline void
pf_set (struct page_frame *pf, int flag)
{
  pf->flags |= flag;
}

static inline void
pf_clr (struct page_frame *pf, int flag)
{
  pf->flags &= ~flag;
}

enum pgr_flag
{
  PF_ISNEW = 1u << 0,
};

struct pager
{
  struct file_pager fp;
  struct wal ww;
  bool restarting;
  struct lockt *lt;
  struct thread_pool *tp;

  struct dpg_table dpt;
  struct txn_table tnxt;

  txid next_tid;

  hash_table_idx pgno_to_value;
  struct page_frame pages[MEMORY_PAGE_LEN];
  hentry_idx _hdata[MEMORY_PAGE_LEN];
  u32 clock;
  bool wal_enabled;

  struct latch l;

  // CACHE
  lsn master_lsn;
  pgno first_tombstone;

  int flags;
};

DEFINE_DBG_ASSERT (
    struct pager, pager, p,
    {
      ASSERT (p);
      ASSERT (p->clock < MEMORY_PAGE_LEN);
    })
