#include "nsfile.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define BUFFER_SIZE 65536
#define VERSION "0.1.0"

typedef enum
{
  CMD_INSERT,
  CMD_WRITE,
  CMD_READ,
  CMD_REMOVE,
  CMD_TAKE
} command_t;

typedef struct
{
  command_t cmd;
  const char *db_path;
  const char *wal_path;
  sb_size offset;         /* For insert */
  struct stride *strides; /* For write/read/remove/take */
  size_t num_strides;
  bool truncate;
  bool json;
  bool quiet;
  bool verbose;
  size_t buffer_size;
} options_t;

/* Forward declarations */
static void print_help (const char *progname);
static void print_version (void);
static int parse_args (int argc, char **argv, options_t *opts);
static int parse_range (const char *range_str, struct stride **strides, size_t *num_strides);
static void ensure_file_exists (const char *path);

static int cmd_insert (const options_t *opts);
static int cmd_write (const options_t *opts);
static int cmd_read (const options_t *opts);
static int cmd_remove (const options_t *opts);
static int cmd_take (const options_t *opts);

static void print_error (const char *msg, error *e);
static void print_json_result (bool success, const char *operation, sb_size bytes, const char *msg);

/* Main entry point */
int
main (int argc, char **argv)
{
  options_t opts = { 0 };
  opts.buffer_size = BUFFER_SIZE;

  int ret = parse_args (argc, argv, &opts);
  if (ret != 0)
    {
      return ret;
    }

  ensure_file_exists (opts.db_path);

  switch (opts.cmd)
    {
    case CMD_INSERT:
      {
        return cmd_insert (&opts);
      }
    case CMD_WRITE:
      {
        return cmd_write (&opts);
      }
    case CMD_READ:
      {
        return cmd_read (&opts);
      }
    case CMD_REMOVE:
      {
        return cmd_remove (&opts);
      }
    case CMD_TAKE:
      {
        return cmd_take (&opts);
      }
    default:
      {
        fprintf (stderr, "Unknown command\n");
        return 1;
      }
    }
}

/* Insert command: read from stdin/file and insert at offset */
static int
cmd_insert (const options_t *opts)
{
  error e = { 0 };
  nsfile *nf = NULL;
  struct txn *tx = NULL;
  FILE *input = NULL;
  void *buffer = NULL;
  int exit_code = 0;

  /* Open database */
  nf = nsfile_open (opts->db_path, opts->wal_path, &e);
  if (!nf)
    {
      print_error ("Failed to open database", &e);
      return 2;
    }

  /* Get initial size */
  sb_size initial_size = nsfile_size (nf, &e);

  /* Begin transaction if WAL provided */
  if (opts->wal_path)
    {
      tx = nsfile_begin_txn (nf, &e);
      if (!tx)
        {
          print_error ("Failed to begin transaction", &e);
          exit_code = 4;
          goto cleanup;
        }
    }

  /* Open stdin for reading */
  input = stdin;

  /* Allocate buffer */
  buffer = malloc (opts->buffer_size);
  if (!buffer)
    {
      fprintf (stderr, "Error: Failed to allocate buffer\n");
      exit_code = 2;
      goto cleanup;
    }

  /* Read and insert data */
  size_t total_bytes = 0;
  size_t nread;

  while ((nread = fread (buffer, 1, opts->buffer_size, input)) > 0)
    {
      err_t err = nsfile_insert (nf, tx, buffer, opts->offset + total_bytes, 1, nread, &e);
      if (err != SUCCESS)
        {
          print_error ("Insert failed", &e);
          exit_code = 2;
          goto cleanup;
        }
      total_bytes += nread;
    }

  if (ferror (input))
    {
      fprintf (stderr, "Error: Failed to read from stdin\n");
      exit_code = 2;
      goto cleanup;
    }

  /* Commit transaction */
  if (tx)
    {
      if (nsfile_commit (nf, tx, &e) != SUCCESS)
        {
          print_error ("Failed to commit transaction", &e);
          exit_code = 4;
          goto cleanup;
        }
    }

  /* Print results */
  sb_size final_size = nsfile_size (nf, &e);

  if (opts->json)
    {
      printf ("{\"status\":\"success\",\"operation\":\"insert\",\"offset\":%" PRb_size ","
              "\"bytes_inserted\":%zu,\"initial_size\":%" PRb_size ",\"final_size\":%" PRb_size "}\n",
              opts->offset, total_bytes, initial_size, final_size);
    }
  else if (!opts->quiet)
    {
      printf ("Inserted %zu bytes at offset %" PRb_size "\n", total_bytes, opts->offset);
      printf ("Database size: %" PRb_size " -> %" PRb_size " bytes\n", initial_size, final_size);
      if (opts->wal_path)
        {
          printf ("Transaction committed\n");
        }
    }

cleanup:
  if (exit_code != 0 && tx)
    {
      nsfile_rollback (nf, tx, &e);
    }
  free (buffer);
  if (nf)
    {
      nsfile_close (nf, &e);
    }

  return exit_code;
}

/* Write command: overwrite bytes in range with stdin data */
static int
cmd_write (const options_t *opts)
{
  error e = { 0 };
  nsfile *nf = NULL;
  struct txn *tx = NULL;
  FILE *input = NULL;
  void *buffer = NULL;
  int exit_code = 0;

  nf = nsfile_open (opts->db_path, opts->wal_path, &e);
  if (!nf)
    {
      print_error ("Failed to open database", &e);
      return 2;
    }

  if (opts->wal_path)
    {
      tx = nsfile_begin_txn (nf, &e);
      if (!tx)
        {
          print_error ("Failed to begin transaction", &e);
          exit_code = 4;
          goto cleanup;
        }
    }

  /* Calculate total bytes needed from strides */
  size_t total_needed = 0;
  for (size_t i = 0; i < opts->num_strides; i++)
    {
      total_needed += opts->strides[i].nelems;
    }

  /* Read all input data */
  buffer = malloc (total_needed);
  if (!buffer)
    {
      fprintf (stderr, "Error: Failed to allocate buffer\n");
      exit_code = 2;
      goto cleanup;
    }

  input = stdin;
  size_t total_read = fread (buffer, 1, total_needed, input);

  /* Check input length */
  if (total_read < total_needed)
    {
      if (!opts->truncate)
        {
          fprintf (stderr, "Error: Input exhausted after %zu bytes (expected %zu)\n",
                   total_read, total_needed);
          exit_code = 3;
          goto cleanup;
        }
      else
        {
          if (!opts->quiet)
            {
              fprintf (stderr, "Warning: Wrote only %zu of %zu bytes (input exhausted)\n",
                       total_read, total_needed);
            }
        }
    }

  /* Perform write operation for each stride */
  size_t bytes_written = 0;
  size_t buffer_offset = 0;

  for (size_t i = 0; i < opts->num_strides && buffer_offset < total_read; i++)
    {
      size_t write_size = opts->strides[i].nelems;
      if (buffer_offset + write_size > total_read)
        {
          write_size = total_read - buffer_offset;
        }

      struct stride single_stride = opts->strides[i];
      single_stride.nelems = write_size;

      err_t err = nsfile_write (nf, tx, (char *)buffer + buffer_offset,
                                write_size, single_stride, &e);
      if (err != SUCCESS)
        {
          print_error ("Write failed", &e);
          exit_code = 2;
          goto cleanup;
        }

      bytes_written += write_size;
      buffer_offset += write_size;
    }

  /* Commit transaction */
  if (tx)
    {
      if (nsfile_commit (nf, tx, &e) != SUCCESS)
        {
          print_error ("Failed to commit transaction", &e);
          exit_code = 4;
          goto cleanup;
        }
    }

  /* Print results */
  if (opts->json)
    {
      printf ("{\"status\":\"success\",\"operation\":\"write\","
              "\"bytes_written\":%zu,\"db_size\":%" PRb_size "}\n",
              bytes_written, nsfile_size (nf, &e));
    }
  else if (!opts->quiet)
    {
      printf ("Wrote %zu bytes\n", bytes_written);
      if (opts->wal_path)
        {
          printf ("Transaction committed\n");
        }
    }

cleanup:
  if (exit_code != 0 && tx)
    {
      nsfile_rollback (nf, tx, &e);
    }
  free (buffer);
  if (nf)
    {
      nsfile_close (nf, &e);
    }

  return exit_code;
}

/* Read command: read bytes from range to stdout */
static int
cmd_read (const options_t *opts)
{
  error e = { 0 };
  nsfile *nf = NULL;
  void *buffer = NULL;
  int exit_code = 0;

  nf = nsfile_open (opts->db_path, opts->wal_path, &e);
  if (!nf)
    {
      print_error ("Failed to open database", &e);
      return 2;
    }

  /* Calculate total bytes to read */
  size_t total_size = 0;
  for (size_t i = 0; i < opts->num_strides; i++)
    {
      total_size += opts->strides[i].nelems;
    }

  buffer = malloc (total_size);
  if (!buffer)
    {
      fprintf (stderr, "Error: Failed to allocate buffer\n");
      exit_code = 2;
      goto cleanup;
    }

  /* Read data for each stride */
  size_t buffer_offset = 0;
  for (size_t i = 0; i < opts->num_strides; i++)
    {
      sb_size bytes_read = nsfile_read (nf, (char *)buffer + buffer_offset,
                                        opts->strides[i].nelems,
                                        opts->strides[i], &e);
      if (bytes_read < 0)
        {
          print_error ("Read failed", &e);
          exit_code = 5;
          goto cleanup;
        }
      buffer_offset += bytes_read;
    }

  /* Write to stdout */
  size_t written = fwrite (buffer, 1, buffer_offset, stdout);
  if (written != buffer_offset)
    {
      fprintf (stderr, "Error: Failed to write to stdout\n");
      exit_code = 2;
      goto cleanup;
    }

cleanup:
  free (buffer);
  if (nf)
    {
      nsfile_close (nf, &e);
    }

  return exit_code;
}

/* Remove command: delete bytes in range, compacting file */
static int
cmd_remove (const options_t *opts)
{
  error e = { 0 };
  nsfile *nf = NULL;
  struct txn *tx = NULL;
  int exit_code = 0;

  nf = nsfile_open (opts->db_path, opts->wal_path, &e);
  if (!nf)
    {
      print_error ("Failed to open database", &e);
      return 2;
    }

  sb_size initial_size = nsfile_size (nf, &e);

  if (opts->wal_path)
    {
      tx = nsfile_begin_txn (nf, &e);
      if (!tx)
        {
          print_error ("Failed to begin transaction", &e);
          exit_code = 4;
          goto cleanup;
        }
    }

  /* Calculate total bytes to remove */
  size_t total_removed = 0;
  for (size_t i = 0; i < opts->num_strides; i++)
    {
      total_removed += opts->strides[i].nelems;
    }

  /* Remove each stride */
  for (size_t i = 0; i < opts->num_strides; i++)
    {
      err_t err = nsfile_remove (nf, tx, NULL, opts->strides[i].nelems,
                                 opts->strides[i], &e);
      if (err != SUCCESS)
        {
          print_error ("Remove failed", &e);
          exit_code = 5;
          goto cleanup;
        }
    }

  /* Commit transaction */
  if (tx)
    {
      if (nsfile_commit (nf, tx, &e) != SUCCESS)
        {
          print_error ("Failed to commit transaction", &e);
          exit_code = 4;
          goto cleanup;
        }
    }

  sb_size final_size = nsfile_size (nf, &e);

  if (opts->json)
    {
      printf ("{\"status\":\"success\",\"operation\":\"remove\","
              "\"bytes_removed\":%zu,\"initial_size\":%" PRb_size ",\"final_size\":%" PRb_size "}\n",
              total_removed, initial_size, final_size);
    }
  else if (!opts->quiet)
    {
      printf ("Removed %zu bytes\n", total_removed);
      printf ("Database size: %" PRb_size " -> %" PRb_size " bytes\n", initial_size, final_size);
      if (opts->wal_path)
        {
          printf ("Transaction committed\n");
        }
    }

cleanup:
  if (exit_code != 0 && tx)
    {
      nsfile_rollback (nf, tx, &e);
    }
  if (nf)
    {
      nsfile_close (nf, &e);
    }

  return exit_code;
}

/* Take command: atomically read and remove bytes */
static int
cmd_take (const options_t *opts)
{
  error e = { 0 };
  nsfile *nf = NULL;
  struct txn *tx = NULL;
  void *buffer = NULL;
  int exit_code = 0;

  nf = nsfile_open (opts->db_path, opts->wal_path, &e);
  if (!nf)
    {
      print_error ("Failed to open database", &e);
      return 2;
    }

  /* Must have WAL for atomic take */
  if (!opts->wal_path)
    {
      fprintf (stderr, "Error: take command requires WAL for atomicity\n");
      exit_code = 1;
      goto cleanup;
    }

  tx = nsfile_begin_txn (nf, &e);
  if (!tx)
    {
      print_error ("Failed to begin transaction", &e);
      exit_code = 4;
      goto cleanup;
    }

  /* Calculate total bytes */
  size_t total_size = 0;
  for (size_t i = 0; i < opts->num_strides; i++)
    {
      total_size += opts->strides[i].nelems;
    }

  buffer = malloc (total_size);
  if (!buffer)
    {
      fprintf (stderr, "Error: Failed to allocate buffer\n");
      exit_code = 2;
      goto cleanup;
    }

  /* Read data first */
  size_t buffer_offset = 0;
  for (size_t i = 0; i < opts->num_strides; i++)
    {
      sb_size bytes_read = nsfile_read (nf, (char *)buffer + buffer_offset,
                                        opts->strides[i].nelems,
                                        opts->strides[i], &e);
      if (bytes_read < 0)
        {
          print_error ("Read failed during take", &e);
          exit_code = 5;
          goto cleanup;
        }
      buffer_offset += bytes_read;
    }

  /* Remove data */
  for (size_t i = 0; i < opts->num_strides; i++)
    {
      err_t err = nsfile_remove (nf, tx, NULL, opts->strides[i].nelems,
                                 opts->strides[i], &e);
      if (err != SUCCESS)
        {
          print_error ("Remove failed during take", &e);
          exit_code = 5;
          goto cleanup;
        }
    }

  /* Commit transaction */
  if (nsfile_commit (nf, tx, &e) != SUCCESS)
    {
      print_error ("Failed to commit transaction", &e);
      exit_code = 4;
      goto cleanup;
    }

  /* Write to stdout only after successful commit */
  size_t written = fwrite (buffer, 1, buffer_offset, stdout);
  if (written != buffer_offset)
    {
      fprintf (stderr, "Error: Failed to write to stdout\n");
      exit_code = 2;
      goto cleanup;
    }

cleanup:
  if (exit_code != 0 && tx)
    {
      nsfile_rollback (nf, tx, &e);
    }
  free (buffer);
  if (nf)
    {
      nsfile_close (nf, &e);
    }

  return exit_code;
}

/* Ensure file exists, create if not */
static void
ensure_file_exists (const char *path)
{
  struct stat st;
  if (stat (path, &st) != 0)
    {
      /* File doesn't exist, create it */
      FILE *f = fopen (path, "wb");
      if (f)
        {
          fclose (f);
        }
    }
}

/* Helper functions */
static void
print_error (const char *msg, error *e)
{
  fprintf (stderr, "Error: %s\n", msg);
  if (e && e->cause_code)
    {
      fprintf (stderr, "  %s\n", e->cause_msg);
    }
}

static void
print_help (const char *progname)
{
  /* Print the help menu from previous message */
  printf ("nfile - Numstore file manipulation tool\n\n");
  printf ("USAGE:\n");
  printf ("    %s <command> [options] <database> [wal]\n\n", progname);
  /* ... rest of help text ... */
}

static void
print_version (void)
{
  printf ("nfile version %s\n", VERSION);
}

/* Stub for argument parsing - needs full implementation */
static int
parse_args (int argc, char **argv, options_t *opts)
{
  if (argc < 3)
    {
      print_help (argv[0]);
      return 1;
    }

  /* TODO: Full argument parsing implementation */
  return 0;
}

/* Stub for range parsing - needs full implementation */
static int
parse_range (const char *range_str, struct stride **strides, size_t *num_strides)
{
  /* TODO: Parse NumPy-style ranges like [0:100], [0:10:20:30], etc. */
  return 0;
}
