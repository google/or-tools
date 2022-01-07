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

//
// Costas Array Problem
//
// Finds an NxN matrix of 0s and 1s, with only one 1 per row,
// and one 1 per column, such that all displacement vectors
// between each pair of 1s are distinct.
//
// This example contains two separate implementations. CostasHard()
// uses hard constraints, whereas CostasSoft() uses a minimizer to
// minimize the number of duplicates.

#include <cstdint>
#include <ctime>
#include <set>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"

ABSL_FLAG(int, minsize, 0, "Minimum problem size.");
ABSL_FLAG(int, maxsize, 0, "Maximum problem size.");
ABSL_FLAG(int, model, 1,
          "Model type: 1 integer variables hard, 2 boolean variables, 3 "
          "boolean_variable soft");
ABSL_FLAG(std::string, params, "", "Sat parameters.");

namespace operations_research {
namespace sat {

// Checks that all pairwise distances are unique and returns all violators
void CheckConstraintViolators(const std::vector<int64_t>& vars,
                              std::vector<int>* const violators) {
  int dim = vars.size();

  // Check that all indices are unique
  for (int i = 0; i < dim; ++i) {
    for (int k = i + 1; k < dim; ++k) {
      if (vars[i] == vars[k]) {
        violators->push_back(i);
        violators->push_back(k);
      }
    }
  }

  // Check that all differences are unique for each level
  for (int level = 1; level < dim; ++level) {
    for (int i = 0; i < dim - level; ++i) {
      const int difference = vars[i + level] - vars[i];

      for (int k = i + 1; k < dim - level; ++k) {
        if (difference == vars[k + level] - vars[k]) {
          violators->push_back(k + level);
          violators->push_back(k);
          violators->push_back(i + level);
          violators->push_back(i);
        }
      }
    }
  }
}

// Check that all pairwise differences are unique
bool CheckCostas(const std::vector<int64_t>& vars) {
  std::vector<int> violators;

  CheckConstraintViolators(vars, &violators);

  return violators.empty();
}

// Computes a Costas Array.
void CostasHard(const int dim) {
  CpModelBuilder cp_model;

  // create the variables
  std::vector<IntVar> vars;
  Domain domain(1, dim);
  for (int i = 0; i < dim; ++i) {
    vars.push_back(
        cp_model.NewIntVar(domain).WithName(absl::StrCat("var_", i)));
  }

  cp_model.AddAllDifferent(vars);

  // Check that the pairwise difference is unique
  for (int i = 1; i < dim; ++i) {
    std::vector<IntVar> subset;
    Domain difference_domain(-dim, dim);

    for (int j = 0; j < dim - i; ++j) {
      IntVar diff = cp_model.NewIntVar(difference_domain);
      subset.push_back(diff);
      cp_model.AddEquality(diff, vars[j + i] - vars[j]);
    }

    cp_model.AddAllDifferent(subset);
  }

  Model model;
  if (!absl::GetFlag(FLAGS_params).empty()) {
    model.Add(NewSatParameters(absl::GetFlag(FLAGS_params)));
  }
  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);

  if (response.status() == CpSolverStatus::OPTIMAL) {
    std::vector<int64_t> costas_matrix;
    std::string output;

    for (int n = 0; n < dim; ++n) {
      const int64_t v = SolutionIntegerValue(response, vars[n]);
      costas_matrix.push_back(v);
      absl::StrAppendFormat(&output, "%3lld", v);
    }

    LOG(INFO) << output << " (" << response.wall_time() << " s)";

    CHECK(CheckCostas(costas_matrix))
        << ": Solution is not a valid Costas Matrix.";
  } else {
    LOG(INFO) << "No solution found.";
  }
}

// Computes a Costas Array.
void CostasBool(const int dim) {
  CpModelBuilder cp_model;

  // create the variables
  std::vector<std::vector<BoolVar>> vars(dim);
  std::vector<std::vector<BoolVar>> transposed_vars(dim);
  for (int i = 0; i < dim; ++i) {
    for (int j = 0; j < dim; ++j) {
      const BoolVar var = cp_model.NewBoolVar();
      vars[i].push_back(var);
      transposed_vars[j].push_back(var);
    }
  }

  for (int i = 0; i < dim; ++i) {
    cp_model.AddEquality(LinearExpr::Sum(vars[i]), 1);
    cp_model.AddEquality(LinearExpr::Sum(transposed_vars[i]), 1);
  }

  // Check that the pairwise difference is unique
  for (int step = 1; step < dim; ++step) {
    for (int diff = 1; diff < dim - 1; ++diff) {
      std::vector<BoolVar> positive_diffs;
      std::vector<BoolVar> negative_diffs;
      for (int var = 0; var < dim - step; ++var) {
        for (int value = 0; value < dim - diff; ++value) {
          const BoolVar pos = cp_model.NewBoolVar();
          const BoolVar neg = cp_model.NewBoolVar();
          positive_diffs.push_back(pos);
          negative_diffs.push_back(neg);
          cp_model.AddBoolOr({Not(vars[var][value]),
                              Not(vars[var + step][value + diff]), pos});
          cp_model.AddBoolOr({Not(vars[var][value + diff]),
                              Not(vars[var + step][value]), neg});
        }
      }
      cp_model.AddLessOrEqual(LinearExpr::Sum(positive_diffs), 1);
      cp_model.AddLessOrEqual(LinearExpr::Sum(negative_diffs), 1);
    }
  }

  Model model;
  if (!absl::GetFlag(FLAGS_params).empty()) {
    model.Add(NewSatParameters(absl::GetFlag(FLAGS_params)));
  }
  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);

  if (response.status() == CpSolverStatus::OPTIMAL) {
    std::vector<int64_t> costas_matrix;
    std::string output;

    for (int n = 0; n < dim; ++n) {
      for (int v = 0; v < dim; ++v) {
        if (SolutionBooleanValue(response, vars[n][v])) {
          costas_matrix.push_back(v + 1);
          absl::StrAppendFormat(&output, "%3lld", v + 1);
          break;
        }
      }
    }

    LOG(INFO) << output << " (" << response.wall_time() << " s)";

    CHECK(CheckCostas(costas_matrix))
        << ": Solution is not a valid Costas Matrix.";
  } else {
    LOG(INFO) << "No solution found.";
  }
}

// Computes a Costas Array with a minimization objective.
void CostasBoolSoft(const int dim) {
  CpModelBuilder cp_model;

  // create the variables
  std::vector<std::vector<BoolVar>> vars(dim);
  std::vector<std::vector<BoolVar>> transposed_vars(dim);
  for (int i = 0; i < dim; ++i) {
    for (int j = 0; j < dim; ++j) {
      const BoolVar var = cp_model.NewBoolVar();
      vars[i].push_back(var);
      transposed_vars[j].push_back(var);
    }
  }

  for (int i = 0; i < dim; ++i) {
    cp_model.AddEquality(LinearExpr::Sum(vars[i]), 1);
    cp_model.AddEquality(LinearExpr::Sum(transposed_vars[i]), 1);
  }

  std::vector<IntVar> all_violations;
  // Check that the pairwise difference is unique
  for (int step = 1; step < dim; ++step) {
    for (int diff = 1; diff < dim - 1; ++diff) {
      std::vector<BoolVar> positive_diffs;
      std::vector<BoolVar> negative_diffs;
      for (int var = 0; var < dim - step; ++var) {
        for (int value = 0; value < dim - diff; ++value) {
          const BoolVar pos = cp_model.NewBoolVar();
          const BoolVar neg = cp_model.NewBoolVar();
          positive_diffs.push_back(pos);
          negative_diffs.push_back(neg);
          cp_model.AddBoolOr({Not(vars[var][value]),
                              Not(vars[var + step][value + diff]), pos});
          cp_model.AddBoolOr({Not(vars[var][value + diff]),
                              Not(vars[var + step][value]), neg});
        }
      }
      const IntVar pos_var =
          cp_model.NewIntVar(Domain(0, positive_diffs.size()));
      const IntVar neg_var =
          cp_model.NewIntVar(Domain(0, negative_diffs.size()));
      cp_model.AddGreaterOrEqual(pos_var, LinearExpr::Sum(positive_diffs) - 1);
      cp_model.AddGreaterOrEqual(neg_var, LinearExpr::Sum(negative_diffs) - 1);
      all_violations.push_back(pos_var);
      all_violations.push_back(neg_var);
    }
  }

  cp_model.Minimize(LinearExpr::Sum(all_violations));

  Model model;
  if (!absl::GetFlag(FLAGS_params).empty()) {
    model.Add(NewSatParameters(absl::GetFlag(FLAGS_params)));
  }
  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);

  if (response.status() == CpSolverStatus::OPTIMAL) {
    std::vector<int64_t> costas_matrix;
    std::string output;

    for (int n = 0; n < dim; ++n) {
      for (int v = 0; v < dim; ++v) {
        if (SolutionBooleanValue(response, vars[n][v])) {
          costas_matrix.push_back(v + 1);
          absl::StrAppendFormat(&output, "%3lld", v + 1);
          break;
        }
      }
    }

    LOG(INFO) << output << " (" << response.wall_time() << " s)";

    CHECK(CheckCostas(costas_matrix))
        << ": Solution is not a valid Costas Matrix.";
  } else {
    LOG(INFO) << "No solution found.";
  }
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetFlag(&FLAGS_logtostderr, true);
  google::InitGoogleLogging(argv[0]);
  absl::ParseCommandLine(argc, argv);

  int min = 1;
  int max = 10;

  if (absl::GetFlag(FLAGS_minsize) != 0) {
    min = absl::GetFlag(FLAGS_minsize);

    if (absl::GetFlag(FLAGS_maxsize) != 0) {
      max = absl::GetFlag(FLAGS_maxsize);
    } else {
      max = min;
    }
  }

  for (int size = min; size <= max; ++size) {
    LOG(INFO) << "Computing Costas Array for dim = " << size;
    if (absl::GetFlag(FLAGS_model) == 1) {
      operations_research::sat::CostasHard(size);
    } else if (absl::GetFlag(FLAGS_model) == 2) {
      operations_research::sat::CostasBool(size);
    } else if (absl::GetFlag(FLAGS_model) == 3) {
      operations_research::sat::CostasBoolSoft(size);
    }
  }

  return EXIT_SUCCESS;
}
