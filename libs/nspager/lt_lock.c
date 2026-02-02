#include "numstore/core/hashing.h"
#include <numstore/core/assert.h>
#include <numstore/core/string.h>
#include <numstore/pager/lt_lock.h>

u32
lt_lock_key (struct lt_lock lock)
{
  char hcode[sizeof (union lt_lock_data) + sizeof (u8)];
  u32 hcodelen = 0;
  u8 _type = lock.type;

  hcodelen += i_memcpy (&hcode[hcodelen], &_type, sizeof (_type));

  switch (lock.type)
    {
    case LOCK_DB:
    case LOCK_ROOT:
    case LOCK_VHP:
      {
        break;
      }
    case LOCK_VAR:
      {
        hcodelen += i_memcpy (&hcode[hcodelen], &lock.data.var_root, sizeof (lock.data.var_root));
        break;
      }
    case LOCK_RPTREE:
      {
        hcodelen += i_memcpy (&hcode[hcodelen], &lock.data.rptree_root, sizeof (lock.data.rptree_root));
        break;
      }
    case LOCK_TMBST:
      {
        hcodelen += i_memcpy (&hcode[hcodelen], &lock.data.tmbst_pg, sizeof (lock.data.tmbst_pg));
        break;
      }
    }

  struct string lock_type_hcode = {
    .data = hcode,
    .len = hcodelen,
  };

  return fnv1a_hash (lock_type_hcode);
}

bool
lt_lock_equal (const struct lt_lock left, const struct lt_lock right)
{
  if (left.type != right.type)
    {
      return false;
    }

  switch (left.type)
    {
    case LOCK_DB:
      {
        return true;
      }
    case LOCK_ROOT:
      {
        return true;
      }
    case LOCK_VHP:
      {
        return true;
      }
    case LOCK_VAR:
      {
        return left.data.var_root == right.data.var_root;
      }
    case LOCK_RPTREE:
      {
        return left.data.rptree_root == right.data.rptree_root;
      }
    case LOCK_TMBST:
      {
        return left.data.tmbst_pg == right.data.tmbst_pg;
      }
    }
  UNREACHABLE ();
}

void
i_print_lt_lock (int log_level, struct lt_lock l)
{
  switch (l.type)
    {
    case LOCK_DB:
      {
        i_printf (log_level, "LOCK_DB\n");
        return;
      }
    case LOCK_ROOT:
      {
        i_printf (log_level, "LOCK_ROOT\n");
        return;
      }
    case LOCK_VHP:
      {
        i_printf (log_level, "LOCK_VHP\n");
        return;
      }
    case LOCK_VAR:
      {
        i_printf (log_level, "LOCK_VAR(%" PRpgno ")\n", l.data.var_root);
        return;
      }
    case LOCK_RPTREE:
      {
        i_printf (log_level, "LOCK_RPTREE(%" PRpgno ")\n", l.data.rptree_root);
        return;
      }
    case LOCK_TMBST:
      {
        i_printf (log_level, "LOCK_TMBST(%" PRpgno ")\n", l.data.tmbst_pg);
        return;
      }
    }
  UNREACHABLE ();
}

bool
get_parent (struct lt_lock *parent, struct lt_lock lock)
{
  switch (lock.type)
    {
    case LOCK_DB:
      {
        return false;
      }
    case LOCK_ROOT:
      {
        parent->type = LOCK_DB;
        parent->data = (union lt_lock_data){ 0 };
        return true;
      }
    case LOCK_VHP:
      {
        parent->type = LOCK_DB;
        parent->data = (union lt_lock_data){ 0 };
        return true;
      }
    case LOCK_VAR:
      {
        parent->type = LOCK_DB;
        parent->data = (union lt_lock_data){ 0 };
        return true;
      }
    case LOCK_RPTREE:
      {
        parent->type = LOCK_DB;
        parent->data = (union lt_lock_data){ 0 };
        return true;
      }
    case LOCK_TMBST:
      {
        parent->type = LOCK_DB;
        parent->data = (union lt_lock_data){ 0 };
        return true;
      }
    }

  UNREACHABLE ();
}
