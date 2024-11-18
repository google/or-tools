// Copyright 2010-2024 Google LLC
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
#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"        // IWYU pragma: keep
#include "ortools/base/path.h"  // IWYU pragma: keep
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "tools/cpp/runfiles/runfiles.h"

namespace operations_research::sat {

namespace {

std::string GetTestDataDir() {
  return file::JoinPath(testing::SrcDir(), "_main/sat/fuzz_testdata");
}

void Solve(const CpModelProto& proto) {
  const CpSolverResponse response =
      operations_research::sat::SolveWithParameters(proto,
                                                    "max_time_in_seconds: 4.0");

  const CpSolverResponse response_no_presolve =
      operations_research::sat::SolveWithParameters(
          proto, "max_time_in_seconds:4.0,cp_model_presolve:false");

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
    .WithDomains(fuzztest::Arbitrary<CpModelProto>())
    .WithSeeds(
        fuzztest::ReadFilesFromDirectory<CpModelProto>(GetTestDataDir()));

}  // namespace
}  // namespace operations_research::sat
