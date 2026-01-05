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
 *   Implements testing.h. Global test infrastructure and coverage tracking state.
 */

#include <numstore/test/testing.h>

#include <numstore/intf/types.h>

#ifndef NTEST
// Global test infrastructure
u64 ntests = 0;
test tests[2048] = { 0 };
int test_ret = 0;

// Coverage infrastructure
coverage_point_info coverage_registry[2048] = { 0 };
coverage_point_state local_coverage_state[2048] = { 0 };
coverage_point_state global_coverage_state[2048] = { 0 };
u64 n_coverage_points = 0;

// Random test time unit (default 1ms)
double randt_unit = 0.001;
#endif
