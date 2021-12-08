// Copyright 2010-2021 Google LLC
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

// Magic sequence problem
//
// Compute a sequence of numbers such that the number of occurrences of i
// in the sequence is equal to the value of the ith number.

#include <cstdint>
#include <cstdlib>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/str_format.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"

ABSL_FLAG(int, size, 50, "Size of the problem.");
ABSL_FLAG(std::string, params, "log_search_progress:true,num_search_workers:8",
          "Sat parameters.");

namespace operations_research {
namespace sat {

void MagicSequence(int size) {
  CHECK_GE(size, 1);
  CpModelBuilder cp_model;

  std::vector<std::vector<BoolVar>> var_domains(size);
  for (int i = 0; i < size; ++i) {
    for (int j = 0; j < size; ++j) {
      var_domains[i].push_back(cp_model.NewBoolVar());
    }
  }

  // Domain constraint on each position.
  for (int i = 0; i < size; ++i) {
    cp_model.AddEquality(LinearExpr::Sum(var_domains[i]), 1);
  }

  // The number of variables equal to j shall be the value of vars[j].
  std::vector<int64_t> values(size);
  std::iota(values.begin(), values.end(), 0);  // [0, 1, 2, .., size - 1]
  std::vector<BoolVar> vars_equal_to_j;

  for (int j = 0; j < size; ++j) {
    vars_equal_to_j.clear();
    for (int i = 0; i < size; ++i) {
      vars_equal_to_j.push_back(var_domains[i][j]);
    }
    cp_model.AddEquality(LinearExpr::ScalProd(var_domains[j], values),
                         LinearExpr::Sum(vars_equal_to_j));
  }

  const CpSolverResponse response =
      SolveWithParameters(cp_model.Build(), absl::GetFlag(FLAGS_params));

  if (response.status() == CpSolverStatus::OPTIMAL ||
      response.status() == CpSolverStatus::FEASIBLE) {
    std::string output = "[";
    for (int i = 0; i < size; ++i) {
      if (i != 0) {
        output.append(", ");
      }
      for (int j = 0; j < size; ++j) {
        if (SolutionBooleanValue(response, var_domains[i][j])) {
          absl::StrAppendFormat(&output, "%d", j);
          break;
        }
      }
    }
    output.append("]");
    LOG(INFO) << "Solution = " << output;
  }
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetFlag(&FLAGS_logtostderr, true);
  google::InitGoogleLogging(argv[0]);
  absl::ParseCommandLine(argc, argv);

  operations_research::sat::MagicSequence(absl::GetFlag(FLAGS_size));
  return EXIT_SUCCESS;
}
