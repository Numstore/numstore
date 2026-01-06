#include "numstore/core/assert.h"
#include <numstore/pager/lt_lock.h>

void
i_print_lt_lock (int log_level, struct lt_lock *l)
{
  switch (l->type)
    {
    case LOCK_DB:
      {
        i_printf (log_level, "|%3s - %" PRtxid "| LOCK_DB\n", gr_lock_mode_name (l->mode), l->tid);
        return;
      }
    case LOCK_ROOT:
      {
        i_printf (log_level, "|%3s - %" PRtxid "| LOCK_ROOT\n", gr_lock_mode_name (l->mode), l->tid);
        return;
      }
    case LOCK_FSTMBST:
      {
        i_printf (log_level, "|%3s - %" PRtxid "| LOCK_FSTMBST\n", gr_lock_mode_name (l->mode), l->tid);
        return;
      }
    case LOCK_MSLSN:
      {
        i_printf (log_level, "|%3s - %" PRtxid "| LOCK_MSLSN\n", gr_lock_mode_name (l->mode), l->tid);
        return;
      }
    case LOCK_VHP:
      {
        i_printf (log_level, "|%3s - %" PRtxid "| LOCK_VHP\n", gr_lock_mode_name (l->mode), l->tid);
        return;
      }
    case LOCK_VHPOS:
      {
        i_printf (log_level, "|%3s - %" PRtxid "| LOCK_VHPOS(%" PRp_size ")\n", gr_lock_mode_name (l->mode), l->tid, l->data.vhpos);
        return;
      }
    case LOCK_VAR:
      {
        i_printf (log_level, "|%3s - %" PRtxid "| LOCK_VAR(%" PRpgno ")\n", gr_lock_mode_name (l->mode), l->tid, l->data.var_root);
        return;
      }
    case LOCK_VAR_NEXT:
      {
        i_printf (log_level, "|%3s - %" PRtxid "| LOCK_VAR_NEXT(%" PRpgno ")\n", gr_lock_mode_name (l->mode), l->tid, l->data.var_root_next);
        return;
      }
    case LOCK_RPTREE:
      {
        i_printf (log_level, "|%3s - %" PRtxid "| LOCK_RPTREE(%" PRpgno ")\n", gr_lock_mode_name (l->mode), l->tid, l->data.rptree_root);
        return;
      }
    case LOCK_TMBST:
      {
        i_printf (log_level, "|%3s - %" PRtxid "| LOCK_TMBST(%" PRpgno ")\n", gr_lock_mode_name (l->mode), l->tid, l->data.tmbst_pg);
        return;
      }
    }
  UNREACHABLE ();
}
