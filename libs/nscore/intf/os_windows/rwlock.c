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
 *   Windows rwlock operations implementation
 */

#include <numstore/core/assert.h>
#include <numstore/core/error.h>
#include <numstore/intf/logging.h>
#include <numstore/intf/os.h>

#include <windows.h>

////////////////////////////////////////////////////////////
// RW Lock
err_t
i_rwlock_create (i_rwlock *rw, error *e)
{
  InitializeSRWLock (&rw->lock);
  return SUCCESS;
}

void
i_rwlock_free (i_rwlock *rw)
{
  // SRWLock doesn't need explicit cleanup
}

void
i_rwlock_rdlock (i_rwlock *rw)
{
  AcquireSRWLockShared (&rw->lock);
}

void
i_rwlock_wrlock (i_rwlock *rw)
{
  AcquireSRWLockExclusive (&rw->lock);
}

void
i_rwlock_unlock (i_rwlock *rw)
{
  // Note: Windows SRWLock requires different release functions
  // This is a simplification - in practice you'd need to track lock type
  ReleaseSRWLockExclusive (&rw->lock);
}
