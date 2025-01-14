#!/usr/bin/env bash
# Copyright 2010-2025 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


source gbash.sh || exit
source module gbash_unit.sh

function test::operations_research_examples::cvrptw_with_refueling() {
  declare -r DIR="${TEST_SRCDIR}/ortools/routing/samples"
  EXPECT_SUCCEED "${DIR}/cvrptw_with_refueling\
    --vrp_use_deterministic_random_seed --cp_random_seed=144"
}

gbash::unit::main "$@"
