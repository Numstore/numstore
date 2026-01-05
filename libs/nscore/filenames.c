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
 *   Implements filenames.h. Cross-platform filename and path utilities.
 */

#include <numstore/core/filenames.h>

#include <numstore/intf/stdlib.h>
#include <numstore/test/testing.h>

// core
const char *
basename (const char *path)
{
  const char *p1 = i_strrchr (path, '/');
  const char *p2 = i_strrchr (path, '\\');
  const char *p = p1 > p2 ? p1 : p2;
  return p ? p + 1 : path;
}

#ifndef NTEST
TEST (TT_UNIT, basename)
{
  test_assert (strcmp (basename ("foo/bar"), "bar") == 0);
  test_assert (strcmp (basename ("/foo/bar/baz.c"), "baz.c") == 0);
  test_assert (strcmp (basename ("/foo/bar/"), "") == 0);
  test_assert (strcmp (basename ("/"), "") == 0);

  test_assert (strcmp (basename ("foo\\bar"), "bar") == 0);
  test_assert (strcmp (basename ("C:\\path\\file.txt"), "file.txt") == 0);
  test_assert (strcmp (basename ("C:\\"), "") == 0);

  test_assert (strcmp (basename ("/mixed\\sep/file"), "file") == 0);
  test_assert (strcmp (basename ("mixed/sep\\file.ext"), "file.ext") == 0);

  test_assert (strcmp (basename ("filename"), "filename") == 0);
  test_assert (strcmp (basename (""), "") == 0);
}
#endif
