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
 *   Hash table operation result types and enumerations. Defines return codes
 *   for insert (success, exists, full) and access (success, doesn't exist) operations.
 */

/* Hash table insert result */
typedef enum
{
  HTIR_SUCCESS,
  HTIR_EXISTS,
  HTIR_FULL,
} hti_res;

/* Hash table access result */
typedef enum
{
  HTAR_SUCCESS,
  HTAR_DOESNT_EXIST,
} hta_res;
