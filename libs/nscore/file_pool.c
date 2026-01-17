#include "config.h"
#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/core/file_pool.h>
#include <numstore/core/latch.h>
#include <numstore/intf/os/file_system.h>
#include <numstore/test/testing.h>

err_t
fpool_init (struct file_pool *dest, const char *base, error *e)
{
  // Check if base directory exists
  bool dir_exists;
  err_t_wrap (i_dir_exists (base, &dir_exists, e), e);
  if (!dir_exists)
    {
      return error_causef (e, ERR_INVALID_ARGUMENT, "Directory: %s doesn't exist", base);
    }

  // Add a trailing slash and copy into temp_fname
  u32 base_len = i_strlen (base);
  bool needs_slash = (base_len > 0 && base[base_len - 1] != '/');
  if (needs_slash)
    {
      dest->baselen = i_snprintf (dest->temp_fname, MAX_FILE_NAME, "%s/numstore/", base);
    }
  else
    {
      dest->baselen = i_snprintf (dest->temp_fname, MAX_FILE_NAME, "%snumstore/", base);
    }

  // Create directory
  err_t_wrap (i_dir_exists (dest->temp_fname, &dir_exists, e), e);
  if (!dir_exists)
    {
      err_t_wrap (i_mkdir (dest->temp_fname, e), e);
    }

  ht_init_fp (&dest->table, dest->data, arrlen (dest->data));
  i_memset (dest->files, 0, sizeof (dest->files));
  dest->clock = 0;
  latch_init (&dest->l);

  for (u32 i = 0; i < arrlen (dest->files); ++i)
    {
      latch_init (&dest->files[i].l);
    }

  return SUCCESS;
}

////////////////////////////////////////////////////////////
/// File Name construction

static const char *
get_extension (u8 page_type)
{
  switch (page_type)
    {
    case 0x00:
      {
        return ".db";
      }
    case 0x01:
      {
        return ".wal";
      }
    default:
      {
        UNREACHABLE ();
      }
    }
}

#define PAGE_TYPE_MASK 0xF0000000ULL // Mask to get the file type of the address
#define FILE_NUM_MASK 0x0FFFFFFFULL  // Mask to get file part of the address
#define PAGE_TYPE_SHIFT 28           // Bits for the file name
#define FOLDER_LEN 2                 // "01"
#define FILENAME_LEN 9               // "000000001" = 2^28 = 268,435,455
#define EXTENSION_LEN 4              // ".wal"

static inline void
fpool_append_folder (struct file_pool *f, u64 addr)
{
  u8 type = (addr & PAGE_TYPE_MASK) >> PAGE_TYPE_SHIFT;
  u32 ofst = f->baselen;
  u32 len = i_snprintf (f->temp_fname + ofst, MAX_FILE_NAME - ofst, "%02u/", type);

  ASSERT (ofst + len < MAX_FILE_NAME);
}

static inline void
fpool_append_file (struct file_pool *f, u64 addr)
{
  u8 type = (addr & PAGE_TYPE_MASK) >> PAGE_TYPE_SHIFT;
  u32 file_num = addr & FILE_NUM_MASK;

  u32 ofst = f->baselen + 2 + 1; // "00/"
  u32 len = i_snprintf (f->temp_fname + ofst, MAX_FILE_NAME - ofst, "%09u%s", file_num, get_extension (type));

  ASSERT (ofst + len < MAX_FILE_NAME);
}

static inline err_t
fpool_open_file (struct file_pool *f, struct i_file *dest, u64 adr, error *e)
{
  fpool_append_folder (f, adr);

  // Check if folder exists
  bool dir_exists;
  err_t_wrap (i_dir_exists (f->temp_fname, &dir_exists, e), e);

  if (!dir_exists)
    {
      err_t_wrap (i_mkdir (f->temp_fname, e), e);
    }

  // Append file name at the end
  fpool_append_file (f, adr);

  // Open the file
  err_t_wrap (i_open_rw (dest, f->temp_fname, e), e);

  return SUCCESS;
}

struct i_file *
fpool_getf (struct file_pool *f, u64 adr, error *e)
{
  latch_lock (&f->l);

  hdata_fp entry;
  switch (ht_get_fp (&f->table, &entry, adr))
    {
    case HTAR_DOESNT_EXIST:
      {
        break;
      }
    case HTAR_SUCCESS:
      {
        struct i_file *ret = &f->files[entry.value].fp;
        latch_lock (&f->files[entry.value].l);
        f->files[entry.value].flag += 1;
        latch_unlock (&f->files[entry.value].l);

        latch_unlock (&f->l);

        return ret;
      }
    }

  // First pass - search for open spot
  for (u32 i = 0; i < arrlen (f->data); ++i)
    {
      u32 k = f->clock;
      latch_lock (&f->files[k].l);

      // Empty - use this spot
      if (f->files[k].flag == 0)
        {
          // Open the file
          if (fpool_open_file (f, &f->files[k].fp, adr, e))
            {
              latch_unlock (&f->files[k].l);
              latch_unlock (&f->l);
              return NULL;
            }

          // Add to hash table
          hdata_fp new_entry = { .key = adr, .value = k };
          ht_insert_expect_fp (&f->table, new_entry);

          // Initialize meta data
          f->files[k].flag = 2;
          f->files[k].addr = adr;
          struct i_file *ret = &f->files[k].fp;
          f->clock = (f->clock + 1) % arrlen (f->files);

          latch_unlock (&f->files[k].l);
          latch_unlock (&f->l);

          return ret;
        }

      latch_unlock (&f->files[k].l);
      f->clock = (f->clock + 1) % arrlen (f->files);
    }

  // Second pass - evict a unused file
  for (u32 i = 0; i < arrlen (f->data); ++i)
    {
      u32 k = f->clock; // FIXED: Save current slot before incrementing
      latch_lock (&f->files[k].l);

      // Empty - use this spot
      if (f->files[k].flag == 1)
        {
          // Remove entry from the hash table
          ht_delete_expect_fp (&f->table, NULL, f->files[k].addr);

          // Evict this file
          if (i_close (&f->files[k].fp, e))
            {
              latch_unlock (&f->files[k].l);
              latch_unlock (&f->l);
              return NULL;
            }

          // Open the file
          if (fpool_open_file (f, &f->files[k].fp, adr, e))
            {
              latch_unlock (&f->files[k].l);
              latch_unlock (&f->l);
              return NULL;
            }

          // Add new entry to hash table
          hdata_fp new_entry = { .key = adr, .value = k };
          ht_insert_expect_fp (&f->table, new_entry);

          // Update entry meta data
          f->files[k].flag = 2;
          f->files[k].addr = adr;
          struct i_file *ret = &f->files[k].fp;
          f->clock = (f->clock + 1) % arrlen (f->files);

          latch_unlock (&f->files[k].l);
          latch_unlock (&f->l);

          return ret;
        }

      latch_unlock (&f->files[k].l);
      f->clock = (f->clock + 1) % arrlen (f->files);
    }

  latch_unlock (&f->l);

  error_causef (e, ERR_TOO_MANY_FILES, "File Pool is Full. Can't open file: %" PRIu64, adr);

  return NULL;
}

void
fpool_release (struct file_pool *f, struct i_file *fp)
{
  struct ifframe *frame = container_of (fp, struct ifframe, fp);
  latch_lock (&frame->l);
  ASSERT (frame->flag > 1);
  frame->flag--;
  latch_unlock (&frame->l);
}

err_t
fpool_close (struct file_pool *f, error *e)
{
  latch_lock (&f->l);
  for (u32 i = 0; i < arrlen (f->files); ++i)
    {
      latch_lock (&f->files[i].l);
      ASSERTF (f->files[i].flag <= 1u, "Failed to close all files before calling fpool_close\n");
      if (f->files[i].flag == 1)
        {
          i_close (&f->files[i].fp, e);
          f->files[i].flag = 0;
        }
      latch_unlock (&f->files[i].l);
    }
  latch_unlock (&f->l);

  return e->cause_code;
}

////////////////////////////////////////////////////////////
/// Tests

#ifndef NTEST

TEST (TT_UNIT, fpool)
{
  TEST_CASE ("fpool_init_state")
  {
    error e = error_create ();
    struct file_pool pool;
    test_err_t_wrap (fpool_init (&pool, "./", &e), &e);

    // All files should be marked as unused (flag = 0)
    for (u32 i = 0; i < arrlen (pool.files); ++i)
      {
        test_assert_int_equal (pool.files[i].flag, 0);
      }

    // Clock should start at 0
    test_assert_int_equal (pool.clock, 0);
  }

  TEST_CASE ("fpool_getf_and_release")
  {
    struct file_pool pool;
    error e = error_create ();
    test_err_t_wrap (fpool_init (&pool, "./", &e), &e);

    // Get a file - should open it
    u64 addr1 = (0ULL << 28) | 1;
    struct i_file *f1 = fpool_getf (&pool, addr1, &e);
    test_fail_if_null (f1);

    // File should be marked as in-use (flag = 2)
    struct ifframe *frame1 = container_of (f1, struct ifframe, fp);
    test_assert_int_equal (frame1->flag, 2);

    // Getting the same file again should return same file and increment flag
    struct i_file *f1_again = fpool_getf (&pool, addr1, &e);
    test_assert_ptr_equal (f1, f1_again);
    test_assert_int_equal (frame1->flag, 3);

    // Release once - flag should decrement
    fpool_release (&pool, f1);
    test_assert_int_equal (frame1->flag, 2);

    // Release again - flag should decrement to 1 (unused but open)
    fpool_release (&pool, f1_again);
    test_assert_int_equal (frame1->flag, 1);

    // Clean up
    test_err_t_wrap (fpool_close (&pool, &e), &e);
  }

  TEST_CASE ("fpool_hash_table_lookup")
  {
    struct file_pool pool;
    error e = error_create ();
    test_err_t_wrap (fpool_init (&pool, "./", &e), &e);

    // Open first file
    u64 addr1 = (0ULL << 28) | 1;
    struct i_file *f1 = fpool_getf (&pool, addr1, &e);
    test_fail_if_null (f1);

    // Open second file
    u64 addr2 = (0ULL << 28) | 2;
    struct i_file *f2 = fpool_getf (&pool, addr2, &e);
    test_fail_if_null (f2);

    // Get first file again - should come from hash table
    struct i_file *f1_again = fpool_getf (&pool, addr1, &e);
    test_fail_if_null (f2);
    test_assert_ptr_equal (f1, f1_again);

    // Clean up
    fpool_release (&pool, f1);
    fpool_release (&pool, f1_again);
    fpool_release (&pool, f2);
    test_err_t_wrap (fpool_close (&pool, &e), &e);
  }

  TEST_CASE ("fpool_eviction")
  {
    struct file_pool pool;
    error e = error_create ();
    test_err_t_wrap (fpool_init (&pool, "./", &e), &e);

    // Fill the pool completely
    struct i_file *files[arrlen (pool.files)];
    for (u32 i = 0; i < arrlen (pool.files); ++i)
      {
        u64 addr = (0ULL << 28) | i;
        files[i] = fpool_getf (&pool, addr, &e);
        test_fail_if_null (files[i]);
      }

    // Release one file (makes it evictable)
    fpool_release (&pool, files[0]);

    // Get a new file - should evict the released one
    u64 new_addr = (0ULL << 28) | arrlen (pool.files);
    struct i_file *new_file = fpool_getf (&pool, new_addr, &e);
    test_fail_if_null (new_file);

    // Old address should no longer be in hash table (was evicted)
    u64 old_addr = (0ULL << 28) | 0;
    hdata_fp check_entry;
    test_assert_int_equal (ht_get_fp (&pool.table, &check_entry, old_addr), HTAR_DOESNT_EXIST);

    // New address should be in hash table
    test_assert_int_equal (ht_get_fp (&pool.table, &check_entry, new_addr), HTAR_SUCCESS);

    // Release all files for cleanup
    for (u32 i = 1; i < arrlen (pool.files); ++i)
      {
        fpool_release (&pool, files[i]);
      }
    fpool_release (&pool, new_file);

    test_err_t_wrap (fpool_close (&pool, &e), &e);
  }

  TEST_CASE ("fpool_full_error")
  {
    struct file_pool pool;
    error e = error_create ();
    test_err_t_wrap (fpool_init (&pool, "./", &e), &e);

    // Fill the pool completely and keep all files pinned
    struct i_file *files[arrlen (pool.files)];
    for (u32 i = 0; i < arrlen (pool.files); ++i)
      {
        u64 addr = (0ULL << 28) | i;
        files[i] = fpool_getf (&pool, addr, &e);
        test_fail_if_null (files[i]);
      }

    // Try to get one more file - should fail with ERR_TOO_MANY_FILES
    u64 overflow_addr = (0ULL << 28) | arrlen (pool.files);
    struct i_file *overflow = fpool_getf (&pool, overflow_addr, &e);
    test_assert_ptr_equal (overflow, NULL);
    test_assert_int_equal (e.cause_code, ERR_TOO_MANY_FILES);

    // Clean up
    error_reset (&e);
    for (u32 i = 0; i < arrlen (pool.files); ++i)
      {
        fpool_release (&pool, files[i]);
      }
    test_err_t_wrap (fpool_close (&pool, &e), &e);
  }

  TEST_CASE ("fpool_clock_advancement")
  {
    struct file_pool pool;
    error e = error_create ();
    test_err_t_wrap (fpool_init (&pool, "./", &e), &e);

    test_assert_int_equal (pool.clock, 0);

    // Open first file - clock should advance
    u64 addr1 = (0ULL << 28) | 1;
    struct i_file *fp1 = fpool_getf (&pool, addr1, &e);
    test_fail_if_null (fp1);
    test_assert_int_equal (pool.clock, 1);

    // Open second file - clock should advance again
    u64 addr2 = (0ULL << 28) | 2;
    struct i_file *fp2 = fpool_getf (&pool, addr2, &e);
    test_fail_if_null (fp2);
    test_assert_int_equal (pool.clock, 2);

    // Getting existing file should NOT advance clock
    struct i_file *fp3 = fpool_getf (&pool, addr1, &e);
    test_fail_if_null (fp3);
    test_assert_int_equal (pool.clock, 2);

    fpool_release (&pool, fp1);
    fpool_release (&pool, fp2);
    fpool_release (&pool, fp3);

    // Clean up
    fpool_close (&pool, &e);
  }
}

#endif
