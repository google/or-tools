// Copyright 2010-2025 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>

#include "absl/log/check.h"
#include "gtest/gtest.h"  // IWYU pragma: keep
#include "ortools/base/fuzztest.h"
#include "ortools/base/path.h"  // IWYU pragma: keep
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "tools/cpp/runfiles/runfiles.h"

namespace operations_research::sat {

namespace {

std::string GetTestDataDir() {
  return file::JoinPathRespectAbsolute(::testing::SrcDir(),
                                       "_main/ortools/sat/fuzz_testdata");
}

void Solve(const CpModelProto& proto) {
  SatParameters params;
  params.set_max_time_in_seconds(4.0);
  params.set_debug_crash_if_presolve_breaks_hint(true);

  // Enable all fancy heuristics.
  params.set_linearization_level(2);
  params.set_use_try_edge_reasoning_in_no_overlap_2d(true);
  params.set_exploit_all_precedences(true);
  params.set_use_hard_precedences_in_cumulative(true);
  params.set_max_num_intervals_for_timetable_edge_finding(1000);
  params.set_use_overload_checker_in_cumulative(true);
  params.set_use_strong_propagation_in_disjunctive(true);
  params.set_use_timetable_edge_finding_in_cumulative(true);
  params.set_max_pairs_pairwise_reasoning_in_no_overlap_2d(50000);
  params.set_use_timetabling_in_no_overlap_2d(true);
  params.set_use_energetic_reasoning_in_no_overlap_2d(true);
  params.set_use_area_energetic_reasoning_in_no_overlap_2d(true);
  params.set_use_conservative_scale_overload_checker(true);
  params.set_use_dual_scheduling_heuristics(true);

  const CpSolverResponse response =
      operations_research::sat::SolveWithParameters(proto, params);

  params.set_cp_model_presolve(false);
  const CpSolverResponse response_no_presolve =
      operations_research::sat::SolveWithParameters(proto, params);

  CHECK_EQ(response.status() == CpSolverStatus::MODEL_INVALID,
           response_no_presolve.status() == CpSolverStatus::MODEL_INVALID)
      << "Model being invalid should not depend on presolve";

  if (response.status() == CpSolverStatus::MODEL_INVALID) {
    return;
  }

  if (response.status() == CpSolverStatus::UNKNOWN ||
      response_no_presolve.status() == CpSolverStatus::UNKNOWN) {
    return;
  }

  CHECK_EQ(response.status() == CpSolverStatus::INFEASIBLE,
           response_no_presolve.status() == CpSolverStatus::INFEASIBLE)
      << "Presolve should not change feasibility";
}

// Fuzzing repeats solve() 100 times, and timeout after 600s.
// With a time limit of 4s, we should be fine.
FUZZ_TEST(CpModelProtoFuzzer, Solve)
    .WithDomains(/*proto:*/ fuzztest::Arbitrary<CpModelProto>())
    .WithSeeds([]() {
      return fuzztest::ReadFilesFromDirectory<CpModelProto>(GetTestDataDir());
    });

}  // namespace
}  // namespace operations_research::sat
