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
  i_file fp; // The open (or closed) file pointer
  u32 flag;  // 1 == present (open), 1 == present (no access), 2 == present (access)
  u64 faddr; // The address that this file represents
  struct latch l;
};

struct file_pool
{
  hash_table_fp table;                  // Hash table to index fd into table
  hentry_fp data[MAX_OPEN_FILES];       // backing for table
  struct ifframe files[MAX_OPEN_FILES]; // the list of files
  u32 clock;                            // Pointer to currently open file
  char temp_fname[MAX_FILE_NAME];       // Temp util file name
  u32 baselen;                          // Length of the base in temp_fname
  struct latch l;
};

// Lifecycle
err_t fpool_init (struct file_pool *dest, const char *base, error *e);
err_t fpool_close (struct file_pool *f, error *e);

err_t fpool_pread (struct file_pool *f, void *dest, u64 n, u64 addr, error *e);
err_t fpool_pwrite (struct file_pool *f, const void *src, u64 n, u64 addr, error *e);

u64 page_to_addr (pgno pg);
u64 lsn_to_addr (lsn l);
