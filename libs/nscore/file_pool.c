#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/core/file_pool.h>
#include <numstore/core/latch.h>
#include <numstore/intf/os/file_system.h>
#include <numstore/test/testing.h>

#include <config.h>

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

// Shifts
#define FILE_TYPE_SHIFT (FILE_NUM_BITS + FILE_OFST_BITS) // How many to shift by to get file type
#define FILE_NUM_SHIFT (FILE_OFST_BITS)                  // How many to shift by to get file number

// Masks
#define FILE_TYPE_MASK (((1ULL << FILE_TYPE_BITS) - 1) << FILE_TYPE_SHIFT) // 111100000...
#define FILE_NUM_MASK (((1ULL << FILE_NUM_BITS) - 1) << FILE_NUM_SHIFT)    // 0000001111111000000....
#define FILE_OFST_MASK ((1ULL << FILE_OFST_BITS) - 1)                      // 0000000000000000000000111111

// Extractors
#define FILE_TYPE(addr) (((addr)&FILE_TYPE_MASK) >> FILE_TYPE_SHIFT)
#define FILE_NUM(addr) (((addr)&FILE_NUM_MASK) >> FILE_NUM_SHIFT)
#define FILE_OFST(addr) ((addr)&FILE_OFST_MASK)

// String lengths - calculate decimal digits needed for max value
// Formula: ceil(N * log10(2)) = ceil(N * 0.30103) ≈ (N * 30103 + 99999) / 100000
#define FILE_TYPE_LEN ((FILE_TYPE_BITS * 30103UL + 99999UL) / 100000UL) // 2
#define FILE_NUM_LEN ((FILE_NUM_BITS * 30103UL + 99999UL) / 100000UL)   // 11

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
  u32 len = i_snprintf (f->temp_fname + ofst, MAX_FILE_NAME - ofst, "%09u", file_num);

  ASSERT (ofst + len < MAX_FILE_NAME);
}

static inline err_t
fpool_open_file (struct file_pool *f, struct i_file *dest, u64 fadr, error *e)
{
  fpool_append_folder (f, fadr);

  // Check if folder exists
  bool exists;
  err_t_wrap (i_dir_exists (f->temp_fname, &exists, e), e);

  if (!exists)
    {
      err_t_wrap (i_mkdir (f->temp_fname, e), e);
    }

  // Append file name at the end
  fpool_append_file (f, fadr);

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

static struct ifframe *
fpool_getf (struct file_pool *f, u64 adr, error *e)
{
  latch_lock (&f->l);

  // Just the file part
  u64 fadr = FILE_TYPE (adr) | FILE_NUM (adr);

  hdata_fp entry;
  switch (ht_get_fp (&f->table, &entry, fadr))
    {
    case HTAR_DOESNT_EXIST:
      {
        break;
      }
    case HTAR_SUCCESS:
      {
        struct ifframe *ret = &f->files[entry.value];
        latch_lock (&f->files[entry.value].l);
        f->files[entry.value].flag = 2;
        latch_unlock (&f->files[entry.value].l);

        latch_unlock (&f->l);

        return ret;
      }
    }

  // First pass
  // If flag == 0 (not present) - read
  for (u32 i = 0; i < arrlen (f->data); ++i)
    {
      u32 k = f->clock;
      latch_lock (&f->files[k].l);

      // Empty - use this spot
      if (f->files[k].flag == 0)
        {
          // Open the file
          if (fpool_open_file (f, &f->files[k].fp, fadr, e))
            {
              latch_unlock (&f->files[k].l);
              latch_unlock (&f->l);
              return NULL;
            }

          // Add to hash table
          hdata_fp new_entry = { .key = fadr, .value = k };
          ht_insert_expect_fp (&f->table, new_entry);

          // Initialize meta data
          f->files[k].flag = 2;
          f->files[k].faddr = fadr;
          struct ifframe *ret = &f->files[k];
          f->clock = (f->clock + 1) % arrlen (f->files);

          latch_unlock (&f->files[k].l);
          latch_unlock (&f->l);

          return ret;
        }

      latch_unlock (&f->files[k].l);
      f->clock = (f->clock + 1) % arrlen (f->files);
    }

  // Second pass
  // If flag == 1 (not accessed) - evict + read
  for (u32 i = 0; i < 2 * arrlen (f->data); ++i)
    {
      u32 k = f->clock;
      latch_lock (&f->files[k].l);

      if (f->files[k].flag == 1)
        {
          // Remove entry from the hash table
          ht_delete_expect_fp (&f->table, NULL, f->files[k].faddr);

          // Evict this file
          if (i_close (&f->files[k].fp, e))
            {
              latch_unlock (&f->files[k].l);
              latch_unlock (&f->l);
              return NULL;
            }

          // Open the file
          if (fpool_open_file (f, &f->files[k].fp, fadr, e))
            {
              latch_unlock (&f->files[k].l);
              latch_unlock (&f->l);
              return NULL;
            }

          // Add new entry to hash table
          hdata_fp new_entry = { .key = fadr, .value = k };
          ht_insert_expect_fp (&f->table, new_entry);

          // Update entry meta data
          f->files[k].flag = 2;
          f->files[k].faddr = fadr;
          struct ifframe *ret = &f->files[k];
          f->clock = (f->clock + 1) % arrlen (f->files);

          latch_unlock (&f->files[k].l);
          latch_unlock (&f->l);

          return ret;
        }

      // Subtract one from this file
      ASSERT (f->files[k].flag == 2);
      f->files[k].flag -= 1;

      latch_unlock (&f->files[k].l);
      f->clock = (f->clock + 1) % arrlen (f->files);
    }

  UNREACHABLE ();
}

err_t
fpool_pread (struct file_pool *f, void *dest, u64 n, u64 addr, error *e)
{
  struct ifframe *frame = fpool_getf (f, addr, e);
  if (frame == NULL)
    {
      return e->cause_code;
    }

  u64 ofst = FILE_OFST (addr);

  return i_pread_all (&frame->fp, dest, n, ofst, e);
}

err_t
fpool_pwrite (struct file_pool *f, const void *src, u64 n, u64 addr, error *e)
{
  struct ifframe *frame = fpool_getf (f, addr, e);
  if (frame == NULL)
    {
      return e->cause_code;
    }

  u64 ofst = FILE_OFST (addr);

  return i_pwrite_all (&frame->fp, src, n, ofst, e);
}

err_t
fpool_close (struct file_pool *f, error *e)
{
  latch_lock (&f->l);
  for (u32 i = 0; i < arrlen (f->files); ++i)
    {
      latch_lock (&f->files[i].l);
      if (f->files[i].flag > 0)
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
