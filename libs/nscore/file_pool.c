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
  bool exists;
  err_t_wrap (i_dir_exists (base, &exists, e), e);
  if (!exists)
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
  err_t_wrap (i_dir_exists (dest->temp_fname, &exists, e), e);
  if (!exists)
    {
      err_t_wrap (i_mkdir (dest->temp_fname, e), e);
    }

  error tmp_err = error_create ();
  ht_init_fp (&dest->table, dest->data, arrlen (dest->data), &tmp_err);
  i_memset (dest->files, 0, sizeof (dest->files));
  dest->clock = 0;

  err_t_wrap (latch_init (&dest->l, e), e);

  for (u32 i = 0; i < arrlen (dest->files); ++i)
    {
      err_t_wrap (latch_init (&dest->files[i].l, e), e);
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

// Shifts
#define FILE_TYPE_SHIFT (FILE_NUM_BITS + FILE_OFST_BITS) // 60
#define FILE_NUM_SHIFT (FILE_OFST_BITS)                  // 24

// Masks
#define FILE_TYPE_MASK (((1ULL << FILE_TYPE_BITS) - 1) << FILE_TYPE_SHIFT) // 0xF000000000000000
#define FILE_NUM_MASK (((1ULL << FILE_NUM_BITS) - 1) << FILE_NUM_SHIFT)    // 0x0FFFFFFFFF000000
#define FILE_OFST_MASK ((1ULL << FILE_OFST_BITS) - 1)                      // 0x0000000000FFFFFF

// Extractors
#define FILE_TYPE(addr) (((addr)&FILE_TYPE_MASK) >> FILE_TYPE_SHIFT)
#define FILE_NUM(addr) (((addr)&FILE_NUM_MASK) >> FILE_NUM_SHIFT)
#define FILE_OFST(addr) ((addr)&FILE_OFST_MASK)

// String lengths - calculate decimal digits needed for max value
// Formula: ceil(N * log10(2)) = ceil(N * 0.30103) ≈ (N * 30103 + 99999) / 100000
#define FILE_TYPE_LEN ((FILE_TYPE_BITS * 30103UL + 99999UL) / 100000UL) // 2
#define FILE_NUM_LEN ((FILE_NUM_BITS * 30103UL + 99999UL) / 100000UL)   // 11
#define EXTENSION_LEN 4                                                 // ".wal"

static inline void
fpool_append_folder (struct file_pool *f, u64 addr)
{
  u8 type = FILE_TYPE (addr);
  u32 ofst = f->baselen;
  u32 len = i_snprintf (f->temp_fname + ofst, MAX_FILE_NAME - ofst, "%02u/", type);

  ASSERT (ofst + len < MAX_FILE_NAME);
}

static inline void
fpool_append_file (struct file_pool *f, u64 addr)
{
  u8 type = FILE_TYPE (addr);
  u32 file_num = FILE_NUM (addr);

  u32 ofst = f->baselen + 2 + 1; // "00/"
  u32 len = i_snprintf (f->temp_fname + ofst, MAX_FILE_NAME - ofst, "%09u%s", file_num, get_extension (type));

  ASSERT (ofst + len < MAX_FILE_NAME);
}

static inline err_t
fpool_open_file (struct file_pool *f, struct i_file *dest, u64 adr, error *e)
{
  fpool_append_folder (f, adr);

  // Check if folder exists
  bool exists;
  err_t_wrap (i_dir_exists (f->temp_fname, &exists, e), e);

  if (!exists)
    {
      err_t_wrap (i_mkdir (f->temp_fname, e), e);
    }

  // Append file name at the end
  fpool_append_file (f, adr);

  // Open the file
  err_t_wrap (i_file_exists (f->temp_fname, &exists, e), e);
  err_t_wrap (i_open_rw (dest, f->temp_fname, e), e);

  if (!exists)
    {
      // Pre allocate segments
      err_t_wrap (i_fallocate (dest, 1 << FILE_OFST_BITS, e), e);
    }

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

u64
page_to_addr (pgno page_num)
{
  u64 byte_offset = page_num << PAGE_POW; // page_num * PAGE_SIZE

  u64 file_num = byte_offset >> FILE_OFST_BITS;
  u32 file_offset = byte_offset & FILE_OFST_MASK;

  return (0ULL << FILE_TYPE_SHIFT) // page_type = 0
         | (file_num << FILE_NUM_SHIFT)
         | file_offset;
}

u64
lsn_to_addr (lsn l)
{
  u64 file_num = l >> FILE_OFST_BITS;
  u32 file_offset = l & FILE_OFST_MASK;

  return (1ULL << FILE_TYPE_SHIFT) // page_type = 1 (WAL)
         | (file_num << FILE_NUM_SHIFT)
         | file_offset;
}

#ifndef NTEST

TEST (TT_UNIT, page_to_addr_conversion)
{
  // Test page 0
  u64 addr = page_to_addr (0);
  test_assert_int_equal (FILE_TYPE (addr), 0);
  test_assert_int_equal (FILE_NUM (addr), 0);
  test_assert_int_equal (FILE_OFST (addr), 0);

  // Test page 2 (offset = 2 * 4096 = 8192)
  addr = page_to_addr (2);
  test_assert_int_equal (FILE_TYPE (addr), 0);
  test_assert_int_equal (FILE_NUM (addr), 0);
  test_assert_int_equal (FILE_OFST (addr), 8192);

  // Test page that fits exactly in first file
  // First file holds: 2^24 bytes / 4096 bytes/page = 4096 pages (0-4095)
  u64 pages_per_file = (1ULL << FILE_OFST_BITS) >> PAGE_POW;
  addr = page_to_addr (pages_per_file - 1); // Last page of first file
  test_assert_int_equal (FILE_TYPE (addr), 0);
  test_assert_int_equal (FILE_NUM (addr), 0);
  test_assert_int_equal (FILE_OFST (addr), (1ULL << FILE_OFST_BITS) - PAGE_SIZE);

  // Test first page of second file (page 4096)
  addr = page_to_addr (pages_per_file);
  test_assert_int_equal (FILE_TYPE (addr), 0);
  test_assert_int_equal (FILE_NUM (addr), 1);
  test_assert_int_equal (FILE_OFST (addr), 0);

  // Test page in second file (page 4097)
  addr = page_to_addr (pages_per_file + 1);
  test_assert_int_equal (FILE_TYPE (addr), 0);
  test_assert_int_equal (FILE_NUM (addr), 1);
  test_assert_int_equal (FILE_OFST (addr), 4096);
}

TEST (TT_UNIT, lsn_to_addr_conversion)
{
  // Test LSN 0
  u64 addr = lsn_to_addr (0);
  test_assert_int_equal (FILE_TYPE (addr), 1);
  test_assert_int_equal (FILE_NUM (addr), 0);
  test_assert_int_equal (FILE_OFST (addr), 0);

  // Test LSN 10
  addr = lsn_to_addr (10);
  test_assert_int_equal (FILE_TYPE (addr), 1);
  test_assert_int_equal (FILE_NUM (addr), 0);
  test_assert_int_equal (FILE_OFST (addr), 10);

  // Test LSN at end of first file
  u64 segment_size = 1ULL << FILE_OFST_BITS;
  addr = lsn_to_addr (segment_size - 1);
  test_assert_int_equal (FILE_TYPE (addr), 1);
  test_assert_int_equal (FILE_NUM (addr), 0);
  test_assert_int_equal (FILE_OFST (addr), segment_size - 1);

  // Test LSN at start of second file
  addr = lsn_to_addr (segment_size);
  test_assert_int_equal (FILE_TYPE (addr), 1);
  test_assert_int_equal (FILE_NUM (addr), 1);
  test_assert_int_equal (FILE_OFST (addr), 0);

  // Test LSN in second file
  addr = lsn_to_addr (segment_size + 100);
  test_assert_int_equal (FILE_TYPE (addr), 1);
  test_assert_int_equal (FILE_NUM (addr), 1);
  test_assert_int_equal (FILE_OFST (addr), 100);

  // Test large LSN (multiple files)
  addr = lsn_to_addr (segment_size * 5 + 12345);
  test_assert_int_equal (FILE_TYPE (addr), 1);
  test_assert_int_equal (FILE_NUM (addr), 5);
  test_assert_int_equal (FILE_OFST (addr), 12345);
}

#endif

// Examples:
// lsn = 10 -> file_num = 10 >> 24 = 0, file_offset = 10
// Result: type=1, file=0, offset=10

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
