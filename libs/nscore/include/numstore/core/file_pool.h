#pragma once

#include <config.h>
#include <numstore/core/latch.h>
#include <numstore/intf/os.h>

#define VTYPE u32
#define KTYPE u64
#define SUFFIX fp
#include <numstore/core/robin_hood_ht.h>
#undef VTYPE
#undef KTYPE
#undef SUFFIX

struct ifframe
{
  i_file fp;
  u32 flag; // 1 == present (open), n - 1 == num owners, 0 == not present
  u64 addr;
  struct latch l;
};

struct file_pool
{
  hash_table_fp table;
  hentry_fp data[MAX_OPEN_FILES];
  struct ifframe files[MAX_OPEN_FILES];
  u32 clock;
  struct latch l;

  char temp_fname[MAX_FILE_NAME];
  u32 baselen;
};

err_t fpool_init (struct file_pool *dest, const char *base, error *e);
struct i_file *fpool_getf (struct file_pool *f, u64 adr, error *e);
void fpool_release (struct file_pool *f, struct i_file *fp);
err_t fpool_close (struct file_pool *f, error *e);
