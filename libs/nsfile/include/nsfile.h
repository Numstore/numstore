#pragma once

#include <unistd.h>

typedef struct nsfile_s nsfile;
typedef struct txn_s txn;

nsfile *nsfile_open (const char *fname, const char *recovery);
int nsfile_close (nsfile *n);

txn *nsfile_begin_txn (nsfile *fd);
int nsfile_commit (nsfile *fd, txn *t);

int nsfile_insert (nsfile *fd, txn *tx, const void *buf, size_t n, __off_t offset);
int nsfile_write (nsfile *fd, txn *tx, int loc, const void *buf, size_t n, __off_t offset);
size_t nsfile_read (nsfile *fd, void *dest, size_t n, __off_t offset);
size_t nsfile_remove (nsfile *fd, txn *tx, size_t ofst, void *dest, size_t n, __off_t offset);
