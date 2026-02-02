#pragma once

#include <numstore/core/error.h>
#include <numstore/intf/types.h>

void print_usage (const char *arg0);
void print_help_short (const char *arg0);
void print_help_long (const char *arg0);

struct nsfilecli_args
{
  enum nsfilecli_command
  {
    NSFCLI_READ,
    NSFCLI_INSERT,
    NSFCLI_WRITE,
    NSFCLI_REMOVE,
    NSFCLI_TAKE
  } command;

  const char *db_file;
  const char *wal_file;

  // For slice-based commands (read, write, remove, take)
  int has_slice;
  sb_size slice_start;
  sb_size slice_step;
  sb_size slice_count;

  // For insert command
  int has_offset;
  sb_size offset;
};

err_t nsfilecli_args_parse (struct nsfilecli_args *dest, int argc, char **argv, error *e);

err_t nsfilecli_execute (struct nsfilecli_args args, error *e);
