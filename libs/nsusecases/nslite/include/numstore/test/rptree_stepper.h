#pragma once

#include <numstore/test/rptree_validator.h>

enum rptv_move_type
{
  RPTV_MOVE_INSERT_SINGLE_START,  // Insert 1 byte at start
  RPTV_MOVE_INSERT_SINGLE_END,    // Insert 1 byte at end
  RPTV_MOVE_INSERT_SINGLE_MIDDLE, // Insert 1 byte at random middle position
  RPTV_MOVE_INSERT_SMALL_START,   // Insert 1-10 bytes at start
  RPTV_MOVE_INSERT_SMALL_END,     // Insert 1-10 bytes at end
  RPTV_MOVE_INSERT_SMALL_MIDDLE,  // Insert 1-10 bytes at random middle
  RPTV_MOVE_INSERT_MEDIUM,        // Insert 10-100 bytes at random position
  RPTV_MOVE_INSERT_LARGE,         // Insert 100-1000 bytes at random position

  RPTV_MOVE_WRITE_SINGLE_START,  // Write 1 byte at start (overwrite)
  RPTV_MOVE_WRITE_SINGLE_END,    // Write 1 byte at end
  RPTV_MOVE_WRITE_SINGLE_MIDDLE, // Write 1 byte at random middle
  RPTV_MOVE_WRITE_SMALL_START,   // Write 1-10 bytes at start
  RPTV_MOVE_WRITE_SMALL_END,     // Write 1-10 bytes at end
  RPTV_MOVE_WRITE_SMALL_MIDDLE,  // Write 1-10 bytes at random middle
  RPTV_MOVE_WRITE_MEDIUM,        // Write 10-100 bytes at random position
  RPTV_MOVE_WRITE_LARGE,         // Write 100-1000 bytes at random position
  RPTV_MOVE_WRITE_STRIDED,       // Write with stride > 1

  RPTV_MOVE_REMOVE_SINGLE_START,  // Remove 1 byte from start
  RPTV_MOVE_REMOVE_SINGLE_END,    // Remove 1 byte from end
  RPTV_MOVE_REMOVE_SINGLE_MIDDLE, // Remove 1 byte from random middle
  RPTV_MOVE_REMOVE_SMALL_START,   // Remove 1-10 bytes from start
  RPTV_MOVE_REMOVE_SMALL_END,     // Remove 1-10 bytes from end
  RPTV_MOVE_REMOVE_SMALL_MIDDLE,  // Remove 1-10 bytes from random middle
  RPTV_MOVE_REMOVE_MEDIUM,        // Remove 10-100 bytes at random position
  RPTV_MOVE_REMOVE_LARGE,         // Remove 100-1000 bytes at random position
  RPTV_MOVE_REMOVE_STRIDED,       // Remove with stride > 1

  RPTV_MOVE_READ_FULL,    // Read entire buffer
  RPTV_MOVE_READ_STRIDED, // Read with stride

  RPTV_MOVE_COUNT
};

struct rptv_stepper
{
  struct rptree_validator *v;
  pgno current_page;
  u64 step_count;
  u32 seed;
};

err_t rptv_stepper_open (struct rptv_stepper *stepper, const char *fname, const char *recovery, u32 seed, error *e);
err_t rptv_stepper_close (struct rptv_stepper *s, error *e);
err_t rptv_stepper_execute (struct rptv_stepper *s, error *e);
