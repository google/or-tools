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

#ifndef ORTOOLS_UTIL_MP_MODEL_SOLUTION_CHECKER_H_
#define ORTOOLS_UTIL_MP_MODEL_SOLUTION_CHECKER_H_

#include <utility>

#include "absl/log/log.h"
#include "absl/types/span.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/util/logging.h"

namespace operations_research {

// Options for the MP model solution checker. Note that all tolerances are 0 by
// default.
struct CheckerOptions {
  double constraint_tolerance = 0.0;
  double variable_tolerance = 0.0;
  double integer_tolerance = 0.0;
  SolverLogger* logger = nullptr;
  int max_logged_violations = 20;
};

// Returns whether the given solution is feasible for the given MPModelProto.
// This is an exact checker, it is a slower than using double arithmetic, so
// you might need something else if you have millions of solutions to check.
//
// constraint_tolerance and variable_tolerance are absolute tolerances used to
// determine whether a constraint or variable bound value is violated.
// Tolerances are strict in the sense that the output is the same as if the
// computation were done with full precision.
// If a logger is provided, detailed logs will be written to it.
//
// Preconditions:
//   model has only linear constraints and indicator constraints;
//   solution.size() == model.variable_size();
//   constraint_tolerance >= 0;
//   variable_tolerance >= 0.
//
// In case of violation of the preconditions, the function returns a status.
absl::StatusOr<bool> SolutionIsFeasible(const MPModelProto& model,
                                        absl::Span<const double> solution,
                                        const CheckerOptions& options);

// Gives a tight interval containing the objective value of the given solution.
absl::StatusOr<std::pair<double, double>> GetTightObjectiveBounds(
    const MPModelProto& model, absl::Span<const double> solution);

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_MP_MODEL_SOLUTION_CHECKER_H_
