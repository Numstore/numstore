#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/core/file_pool.h>
#include <numstore/core/latch.h>
#include <numstore/intf/os/file_system.h>
#include <numstore/test/testing.h>

err_t
fpool_init (struct file_pool *dest, const char *base, error *e)
{
  bool base_exists;
  err_t_wrap (i_dir_exists (base, &base_exists, e), e);
  if (!base_exists)
    {
      return error_causef (e, ERR_INVALID_ARGUMENT, "Directory: %s doesn't exist", base);
    }

  u32 base_len = i_strlen (base);
  bool needs_slash = (base_len > 0 && base[base_len - 1] != '/');
  if (needs_slash)
    {
      i_snprintf (dest->temp_fname, MAX_FILE_NAME, "%s/numstore", base);
    }
  else
    {
      i_snprintf (dest->temp_fname, MAX_FILE_NAME, "%snumstore", base);
    }

  bool db_folder_exists;
  err_t_wrap (i_dir_exists (dest->temp_fname, &db_folder_exists, e), e);
  if (!db_folder_exists) // FIXED: was checking if exists, should check if doesn't exist
    {
      err_t_wrap (i_mkdir (dest->temp_fname, e), e);
    }

  dest->base = base;

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
construct_fname (const char *base, char dest[MAX_FILE_NAME], u64 addr)
{
  u8 page_type = (addr & PAGE_TYPE_MASK) >> PAGE_TYPE_SHIFT;
  u32 file_num = addr & FILE_NUM_MASK;
  const char *ext = get_extension (page_type);

  u32 base_len = i_strlen (base);
  bool needs_slash = (base_len > 0 && base[base_len - 1] != '/');

  if (needs_slash)
    {
      ASSERT (i_strlen (base) + i_strlen ("/numstore//") + FOLDER_LEN + FILENAME_LEN + EXTENSION_LEN < MAX_FILE_NAME);
      i_snprintf (dest, MAX_FILE_NAME, "%s/numstore/%02u/%09u%s", base, page_type, file_num, ext);
    }
  else
    {
      ASSERT (i_strlen (base) + i_strlen ("numstore//") + FOLDER_LEN + FILENAME_LEN + EXTENSION_LEN < MAX_FILE_NAME);
      i_snprintf (dest, MAX_FILE_NAME, "%snumstore/%02u/%09u%s", base, page_type, file_num, ext);
    }
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
          construct_fname (f->base, f->temp_fname, adr);

          // Open the file
          if (i_open_rw (&f->files[k].fp, f->temp_fname, e))
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

          // Open a file here
          construct_fname (f->base, f->temp_fname, adr);

          if (i_open_rw (&f->files[k].fp, f->temp_fname, e))
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

TEST (TT_UNIT, construct_fname)
{
  TEST_CASE ("construct_fname_basic")
  {
    char dest[MAX_FILE_NAME];

    // Test page_type=0 (db), file_num=1
    u64 addr = (0ULL << 28) | 1;
    construct_fname ("/home/theo", dest, addr);
    test_assert_str_equal (dest, "/home/theo/numstore/00/000000001.db");

    // Test page_type=1 (wal), file_num=42
    addr = (1ULL << 28) | 42;
    construct_fname ("/home/theo", dest, addr);
    test_assert_str_equal (dest, "/home/theo/numstore/01/000000042.wal");

    // Test max file number (2^28 - 1 = 268435455)
    addr = (0ULL << 28) | 268435455;
    construct_fname ("/var/lib", dest, addr);
    test_assert_str_equal (dest, "/var/lib/numstore/00/268435455.db");
  }

  TEST_CASE ("construct_fname_trailing_slash")
  {
    char dest[MAX_FILE_NAME];

    // Base with trailing slash
    u64 addr = (1ULL << 28) | 100;
    construct_fname ("/home/theo/", dest, addr);
    test_assert_str_equal (dest, "/home/theo/numstore/01/000000100.wal");

    // Base without trailing slash should produce same result
    char dest2[MAX_FILE_NAME];
    construct_fname ("/home/theo", dest2, addr);
    test_assert_str_equal (dest, dest2);
  }

  TEST_CASE ("construct_fname_edge_cases")
  {
    char dest[MAX_FILE_NAME];

    // File number 0
    u64 addr = (0ULL << 28) | 0;
    construct_fname ("/tmp", dest, addr);
    test_assert_str_equal (dest, "/tmp/numstore/00/000000000.db");

    // Empty base path
    addr = (1ULL << 28) | 5;
    construct_fname ("", dest, addr);
    test_assert_str_equal (dest, "numstore/01/000000005.wal");
  }
}

TEST (TT_UNIT, fpool)
{
  TEST_CASE ("fpool_init_state")
  {
    struct file_pool pool;
    fpool_init (&pool, "/test/base");

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
    fpool_init (&pool, "/tmp/numstore_test");

    // Create test directory structure
    test_err_t_wrap (i_mkdir ("/tmp/numstore_test/numstore/00", &e), &e);

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
    fpool_init (&pool, "/tmp/numstore_test");

    // Create test directory structure
    test_err_t_wrap (i_mkdir ("/tmp/numstore_test/numstore/00", &e), &e);

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
    fpool_init (&pool, "/tmp/numstore_test");

    // Create test directory structure
    test_err_t_wrap (i_mkdir ("/tmp/numstore_test/numstore/00", &e), &e);

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
    fpool_init (&pool, "/tmp/numstore_test");

    // Create test directory structure
    test_err_t_wrap (i_mkdir ("/tmp/numstore_test/numstore/00", &e), &e);

    // Fill the pool completely and keep all files pinned
    struct i_file *files[arrlen (pool.files)];
    for (u32 i = 0; i < arrlen (pool.files); ++i)
      {
        u64 addr = (0ULL << 28) | i;
        test_err_t_wrap (files[i] = fpool_getf (&pool, addr, &e), &e);
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
    fpool_init (&pool, "/tmp/numstore_test");

    // Create test directory structure
    test_err_t_wrap (i_mkdir ("/tmp/numstore_test/numstore/00", &e), &e);

    test_assert_int_equal (pool.clock, 0);

    // Open first file - clock should advance
    u64 addr1 = (0ULL << 28) | 1;
    test_err_t_wrap (fpool_getf (&pool, addr1, &e), &e);
    test_assert_int_equal (pool.clock, 1);

    // Open second file - clock should advance again
    u64 addr2 = (0ULL << 28) | 2;
    test_err_t_wrap (fpool_getf (&pool, addr2, &e), &e);
    test_assert_int_equal (pool.clock, 2);

    // Getting existing file should NOT advance clock
    test_err_t_wrap (fpool_getf (&pool, addr1, &e), &e);
    test_assert_int_equal (pool.clock, 2);

    // Clean up
    fpool_close (&pool, &e);
  }
}

#endif
