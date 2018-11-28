// Copyright 2010-2018 Google LLC
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
#include <vector>

#include "absl/strings/str_format.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"

DEFINE_int32(size, 7, "Size of the magic square");
DEFINE_string(params, "", "Sat paramters");

namespace operations_research {
namespace sat {

void MagicSquare(int size) {
  CpModelBuilder builder;

  std::vector<std::vector<IntVar>> square(size);
  std::vector<std::vector<IntVar>> transposed(size);
  std::vector<IntVar> diag1;
  std::vector<IntVar> diag2;
  std::vector<IntVar> all_variables;
  Domain domain(1, size * size);
  for (int i = 0; i < size; ++i) {
    for (int j = 0; j < size; ++j) {
      const IntVar var = builder.NewIntVar(domain);
      square[i].push_back(var);
      transposed[j].push_back(var);
      all_variables.push_back(var);
      if (i == j) {
        diag1.push_back(var);
      }
      if (i + j == size) {
        diag2.push_back(var);
      }
    }
  }

  // All Diff.
  for (int i = 0; i < size; ++i) {
    builder.AddAllDifferent(all_variables);
  }

  const int sum = size * (size * size + 1) / 2;
  // Sum on rows.
  for (int i = 0; i < size; ++i) {
    builder.AddEquality(LinearExpr::Sum(square[i]), sum);
  }

  // Sum on columns.
  for (int i = 0; i < size; ++i) {
   builder.AddEquality(LinearExpr::Sum(transposed[i]), sum);
  }

  // Sum on diagonals.
  builder.AddEquality(LinearExpr::Sum(diag1), sum);
  builder.AddEquality(LinearExpr::Sum(diag2), sum);

  Model model;
  model.Add(NewSatParameters(FLAGS_params));

  const CpSolverResponse response = SolveWithModel(builder, &model);

  if (response.status() == CpSolverStatus::FEASIBLE) {
    for (int n = 0; n < size; ++n) {
      std::string output;
      for (int m = 0; m < size; ++m) {
        absl::StrAppendFormat(&output, "%3d ",
                              SolutionIntegerValue(response, square[n][m]));
      }
      LOG(INFO) << output;
    }
  } else {
    LOG(INFO) << "No solution found!";
  }
  LOG(INFO) << CpSolverResponseStats(response);
}

} // namespace sat
} // namespace sat

int main(int argc, char **argv) {
  absl::SetFlag(&FLAGS_logtostderr, true);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::sat::MagicSquare(FLAGS_size);
  return EXIT_SUCCESS;
}
