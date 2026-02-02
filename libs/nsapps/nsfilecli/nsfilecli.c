#include "numstore/compiler/lexer.h"
#include "numstore/compiler/parser/stride.h"
#include "numstore/core/error.h"
#include <nsfilecli.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
print_usage (const char *program_name)
{
  fprintf (stderr, "Usage: %s <command> <db_file> [wal_file] [args]\n", program_name);
  fprintf (stderr, "Commands: read, insert, write, remove, take\n");
  fprintf (stderr, "Try '%s -h' for more information\n", program_name);
}

void
print_help_short (const char *program_name)
{
  fprintf (stderr, "Usage: %s <command> <db_file> [wal_file] [args]\n\n", program_name);
  fprintf (stderr, "Commands:\n");
  fprintf (stderr, "  read   <db> [wal] [slice ]        Read records from slice (default: all)\n");
  fprintf (stderr, "  insert <db> [wal] [offset]        Insert records at index from stdin (default: end)\n");
  fprintf (stderr, "  write  <db> [wal] [slice ]        Overwrite records at index from stdin (default: start)\n");
  fprintf (stderr, "  remove <db> [wal] [slice ]        Remove records in slice (default: all)\n");
  fprintf (stderr, "  take   <db> [wal] [slice ]        Remove and output records in slice (default: all)\n\n");
  fprintf (stderr, "Slice format: \"[start:step:count]\" (e.g., \"[0:10:100]\")\n");
  fprintf (stderr, "WAL file is optional - omit for no crash recovery\n");
  fprintf (stderr, "Use '%s --help' for detailed information\n", program_name);
}

void
print_help_long (const char *program_name)
{
  printf ("nsfile - NumStore database file manipulation utility\n\n");
  printf ("USAGE:\n");
  printf ("  %s <command> <db_file> [wal_file] [args]\n\n", program_name);

  printf ("COMMANDS:\n");
  printf ("  read <db> [wal] [slice]\n");
  printf ("      Read records specified by slice and write to stdout\n");
  printf ("      If slice omitted, reads all records\n");
  printf ("      Example: %s read test.db test.wal \"[0:10:100]\" > out\n", program_name);
  printf ("      Example: %s read test.db > all.out\n\n", program_name);

  printf ("  insert <db> [wal] [offset]\n");
  printf ("      Insert records from stdin at specified offset\n");
  printf ("      If offset omitted, appends to end\n");
  printf ("      Example: cat file | %s insert test.db test.wal 10\n", program_name);
  printf ("      Example: cat file | %s insert test.db\n\n", program_name);

  printf ("  write <db> [wal] [slice]\n");
  printf ("      Overwrite records from stdin starting at slice\n");
  printf ("      If slice omitted, starts at beginning (index 0)\n");
  printf ("      Example: cat file | %s write test.db test.wal \"[10:1:5]\"\n", program_name);
  printf ("      Example: cat file | %s write test.db\n\n", program_name);

  printf ("  remove <db> [wal] [slice]\n");
  printf ("      Remove records specified by slice\n");
  printf ("      If slice omitted, removes all records\n");
  printf ("      Example: %s remove test.db test.wal \"[0:10:100]\"\n", program_name);
  printf ("      Example: %s remove test.db\n\n", program_name);

  printf ("  take <db> [wal] [slice]\n");
  printf ("      Remove records specified by slice and write to stdout\n");
  printf ("      If slice omitted, takes all records\n");
  printf ("      Example: %s take test.db test.wal \"[0:10:100]\" > out\n", program_name);
  printf ("      Example: %s take test.db > all.out\n\n", program_name);

  printf ("SLICE NOTATION:\n");
  printf ("  Format: \"[start:step:count]\"\n");
  printf ("    start - Starting index (0-based)\n");
  printf ("    step  - Stride between records\n");
  printf ("    count - Number of records to process\n");
  printf ("  Example: \"[0:10:100]\" processes 100 records starting at 0, every 10th record\n");
  printf ("  If omitted, operates on all records in the database\n\n");

  printf ("ARGUMENTS:\n");
  printf ("  <db_file>   Path to database file (required)\n");
  printf ("  [wal_file]  Path to write-ahead log file (optional)\n");
  printf ("              Omit for no crash recovery\n");
  printf ("  [offset]    Integer record offset for insert operations (optional)\n");
  printf ("              Default: insert=end\n");
  printf ("  [slice]     Slice specification for read/write/remove/take (optional, must be quoted)\n");
  printf ("              Default: all records, write starts at 0\n\n");

  printf ("OPTIONS:\n");
  printf ("  -h          Show short help message\n");
  printf ("  --help      Show this detailed help message\n");
}

static err_t
parse_slice (sb_size *start, sb_size *step, sb_size *stop, const char *data, u32 dlen, error *e)
{
  struct lexer l;
  err_t_wrap (lex_tokens (data, dlen, &l, e), e);

  struct stride_parser p;
  err_t_wrap (parse_stride (l.tokens, l.ntokens, &p, e), e);

  if (p.has_start)
    {
      *start = p.start;
    }
  else
    {
      *start = 0;
    }

  if (p.has_step)
    {
      *step = p.step;
    }
  else
    {
      *step = 1;
    }

  if (p.has_stop)
    {
      *stop = p.stop;
    }
  else
    {
      *stop = -1;
    }

  return SUCCESS;
}

err_t
nsfilecli_args_parse (struct nsfilecli_args *dest, int argc, char **argv, error *e)
{
  if (argc < 2)
    {
      print_usage (argv[0]);
      return error_causef (e, ERR_INVALID_ARGUMENT, "No command specified");
    }

  const char *command = argv[1];

  // Handle help flags
  if (strcmp (command, "-h") == 0)
    {
      print_help_short (argv[0]);
      exit (0);
    }

  if (strcmp (command, "--help") == 0)
    {
      print_help_long (argv[0]);
      exit (0);
    }

  // Parse command type
  if (strcmp (command, "read") == 0)
    {
      dest->command = NSFCLI_READ;
    }
  else if (strcmp (command, "insert") == 0)
    {
      dest->command = NSFCLI_INSERT;
    }
  else if (strcmp (command, "write") == 0)
    {
      dest->command = NSFCLI_WRITE;
    }
  else if (strcmp (command, "remove") == 0)
    {
      dest->command = NSFCLI_REMOVE;
    }
  else if (strcmp (command, "take") == 0)
    {
      dest->command = NSFCLI_TAKE;
    }
  else
    {
      print_usage (argv[0]);
      return SUCCESS;
    }

  // Initialize defaults
  dest->db_file = NULL;
  dest->wal_file = NULL;
  dest->has_slice = 0;
  dest->slice_start = 0;
  dest->slice_step = 1;
  dest->slice_count = 0;
  dest->offset = 0;
  dest->has_offset = 0;

  int i = 2;

  // All commands require at least db_file
  if (i >= argc)
    {
      print_usage (argv[0]);
      return error_causef (e, ERR_INVALID_ARGUMENT, "Missing database file");
    }

  dest->db_file = argv[i++];

  // Check if next argument is a WAL file or another argument
  // WAL files typically end in .wal, but we'll just check if it exists and isn't a slice
  if (i < argc && argv[i][0] != '[')
    {
      // Could be WAL file or offset
      // If it's a number and command is insert, it's an offset
      char *endptr;
      long val = strtol (argv[i], &endptr, 10);

      if (*endptr == '\0' && dest->command == NSFCLI_INSERT)
        {
          // It's an offset for insert
          dest->offset = (sb_size)val;
          dest->has_offset = 1;
          i++;
        }
      else
        {
          // It's a WAL file
          dest->wal_file = argv[i++];
        }
    }

  // Parse command-specific arguments
  switch (dest->command)
    {
    case NSFCLI_READ:
    case NSFCLI_REMOVE:
    case NSFCLI_TAKE:
      // These take optional slice
      if (i < argc)
        {
          err_t_wrap (parse_slice (&dest->slice_start, &dest->slice_step, &dest->slice_count, argv[i], strlen (argv[i]), e), e);
          dest->has_slice = 1;
          i++;
        }
      break;

    case NSFCLI_INSERT:
      // Check if we have an offset (either after db or after wal)
      if (i < argc)
        {
          char *endptr;
          long val = strtol (argv[i], &endptr, 10);
          if (*endptr == '\0')
            {
              dest->offset = (sb_size)val;
              dest->has_offset = 1;
              i++;
            }
        }
      break;

    case NSFCLI_WRITE:
      // Takes optional slice
      if (i < argc)
        {
          err_t_wrap (parse_slice (&dest->slice_start, &dest->slice_step, &dest->slice_count, argv[i], strlen (argv[i]), e), e);
          dest->has_slice = 1;
          i++;
        }
      break;
    }

  // Check for extra arguments
  if (i < argc)
    {
      print_usage (argv[0]);
      return error_causef (e, ERR_INVALID_ARGUMENT, "Unexpected argument: %s", argv[i]);
    }

  return SUCCESS;
}

/**
err_t
nsfilecli_execute (struct nsfilecli_args args, error *e)
{
  switch (args.command)
    {
    case NSFCLI_READ:
      {
        nsfile *f = nsfile_open (args.db_file, args.wal_file, e);
        if (f == NULL)
          {
            return e->cause_code;
          }
        nsfile_read (nsfile * n, void *dest, t_size size, struct stride stride, error *e)
      }
    case NSFCLI_INSERT:
      {
      }
    case NSFCLI_WRITE:
      {
      }
    case NSFCLI_REMOVE:
      {
      }
    case NSFCLI_TAKE:
      {
      }
    }
}
*/
