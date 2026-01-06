#include "numstore/core/hashing.h"
#include <numstore/core/assert.h>
#include <numstore/core/string.h>
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

void
lt_lock_init_key_from_txn (struct lt_lock *dest)
{
  ASSERT (dest);

  char hcode[sizeof (dest->data) + sizeof (u8)];
  hcode[0] = dest->type;
  u32 hcodelen = 1;

  switch (dest->type)
    {
    case LOCK_DB:
    case LOCK_ROOT:
    case LOCK_FSTMBST:
    case LOCK_MSLSN:
    case LOCK_VHP:
    case LOCK_VHPOS:
      {
        hcodelen += i_memcpy (&hcode[hcodelen], &dest->data.vhpos, sizeof (dest->data.vhpos));
        break;
      }
    case LOCK_VAR:
      {
        hcodelen += i_memcpy (&hcode[hcodelen], &dest->data.vhpos, sizeof (dest->data.vhpos));
        break;
      }
    case LOCK_VAR_NEXT:
      {
        hcodelen += i_memcpy (&hcode[hcodelen], &dest->data.var_root_next, sizeof (dest->data.var_root_next));
        break;
      }
    case LOCK_RPTREE:
      {
        hcodelen += i_memcpy (&hcode[hcodelen], &dest->data.rptree_root, sizeof (dest->data.rptree_root));
        break;
      }
    case LOCK_TMBST:
      {
        hcodelen += i_memcpy (&hcode[hcodelen], &dest->data.tmbst_pg, sizeof (dest->data.tmbst_pg));
        break;
      }
    }

  dest->type = dest->type;
  dest->data = dest->data;

  struct cstring lock_type_hcode = {
    .data = hcode,
    .len = hcodelen,
  };

  hnode_init (&dest->lock_type_node, fnv1a_hash (lock_type_hcode));
}

void
lt_lock_init_key (struct lt_lock *dest, enum lt_lock_type type, union lt_lock_data data)
{
  ASSERT (dest);
  dest->type = type;
  dest->data = data;
  lt_lock_init_key_from_txn (dest);
}

bool
lt_lock_eq (const struct hnode *left, const struct hnode *right)
{
  const struct lt_lock *_left = container_of (left, struct lt_lock, lock_type_node);
  const struct lt_lock *_right = container_of (right, struct lt_lock, lock_type_node);

  if (_left->type != _right->type)
    {
      return false;
    }

  switch (_left->type)
    {
    case LOCK_DB:
      {
        return true;
      }
    case LOCK_ROOT:
      {
        return true;
      }
    case LOCK_FSTMBST:
      {
        return true;
      }
    case LOCK_MSLSN:
      {
        return true;
      }
    case LOCK_VHP:
      {
        return true;
      }
    case LOCK_VHPOS:
      {
        return _left->data.vhpos == _right->data.vhpos;
      }
    case LOCK_VAR:
      {
        return _left->data.var_root == _right->data.var_root;
      }
    case LOCK_VAR_NEXT:
      {
        return _left->data.var_root_next == _right->data.var_root_next;
      }
    case LOCK_RPTREE:
      {
        return _left->data.rptree_root == _right->data.rptree_root;
      }
    case LOCK_TMBST:
      {
        return _left->data.tmbst_pg == _right->data.tmbst_pg;
      }
    }
  UNREACHABLE ();
}
