#pragma once

/*
 * Copyright 2025 Theo Lincke
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 *   Internal file pager interface for low-level file I/O operations on database and WAL files.
 */

// core
#include <numstore/core/error.h>
#include <numstore/intf/os.h>

struct file_pager
{
  pgno npages;
  i_file f;
};

err_t fpgr_open (struct file_pager *dest, const char *fname, error *e);
err_t fpgr_close (struct file_pager *f, error *e);
err_t fpgr_reset (struct file_pager *f, error *e);

p_size fpgr_get_npages (const struct file_pager *fp);
err_t fpgr_new (struct file_pager *p, pgno *pgno_dest, error *e);
err_t fpgr_read (struct file_pager *p, u8 *dest, pgno pgno, error *e);
err_t fpgr_write (struct file_pager *p, const u8 *src, pgno pgno, error *e);
err_t fpgr_delete (struct file_pager *p, pgno pgno, error *e);

#ifndef NTEST
err_t fpgr_crash (struct file_pager *p, error *e);
#endif
