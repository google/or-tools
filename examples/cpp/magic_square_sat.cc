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
#include <vector>

#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/base/types.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"

ABSL_FLAG(int, size, 7, "Size of the magic square");
ABSL_FLAG(std::string, params, "", "Sat parameters");

namespace operations_research {
namespace sat {

void MagicSquare(int size) {
  CpModelBuilder builder;

  std::vector<std::vector<IntVar>> square(size);
  std::vector<IntVar> all_variables;
  const Domain domain(1, size * size);
  for (int i = 0; i < size; ++i) {
    for (int j = 0; j < size; ++j) {
      const IntVar var = builder.NewIntVar(domain);
      square[i].push_back(var);
      all_variables.push_back(var);
    }
  }

  // All cells take different values.
  builder.AddAllDifferent(all_variables);

  // The sum on each row, columns and two main diagonals.
  const int magic_value = size * (size * size + 1) / 2;

  // Sum on rows.
  for (int i = 0; i < size; ++i) {
    LinearExpr sum;
    for (int j = 0; j < size; ++j) {
      sum += square[i][j];
    }
    builder.AddEquality(sum, magic_value);
  }

  // Sum on columns.
  for (int j = 0; j < size; ++j) {
    LinearExpr sum;
    for (int i = 0; i < size; ++i) {
      sum += square[i][j];
    }
    builder.AddEquality(sum, magic_value);
  }

  // Sum on diagonals.
  LinearExpr diag1_sum;
  LinearExpr diag2_sum;
  for (int i = 0; i < size; ++i) {
    diag1_sum += square[i][i];
    diag2_sum += square[i][size - 1 - i];
  }
  builder.AddEquality(diag1_sum, magic_value);
  builder.AddEquality(diag2_sum, magic_value);

  Model model;
  model.Add(NewSatParameters(absl::GetFlag(FLAGS_params)));

  const CpSolverResponse response = SolveCpModel(builder.Build(), &model);

  if (response.status() == CpSolverStatus::OPTIMAL) {
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

}  // namespace sat
}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetFlag(&FLAGS_stderrthreshold, 0);
  InitGoogle(argv[0], &argc, &argv, true);

  operations_research::sat::MagicSquare(absl::GetFlag(FLAGS_size));
  return EXIT_SUCCESS;
}
