// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_CPP_STATISTICS_H_
#define OR_TOOLS_MATH_OPT_CPP_STATISTICS_H_

#include <iostream>
#include <optional>
#include <ostream>
#include <utility>

#include "ortools/math_opt/cpp/model.h"

namespace operations_research::math_opt {

// A range of values, first is the minimum, second is the maximum.
using Range = std::pair<double, double>;

// The ranges of the absolute values of the finite non-zero values in the model.
//
// Each range is optional since there may be no finite non-zero values
// (e.g. empty model, empty objective, all variables unbounded, ...).
struct ModelRanges {
  // The linear and quadratic objective terms (not including the offset).
  std::optional<Range> objective_terms;

  // The variables' lower and upper bounds.
  std::optional<Range> variable_bounds;

  // The linear constraints' lower and upper bounds.
  std::optional<Range> linear_constraint_bounds;

  // The coefficients of the variables in linear constraints.
  std::optional<Range> linear_constraint_coefficients;
};

// Prints the ranges with std::setprecision(2) and std::scientific (restoring
// the stream flags after printing).
//
// It prints a multi-line table list of ranges. The last line does NOT end with
// a new line thus the caller should use std::endl if appropriate (see example).
//
// Example:
//
//   std::cout << "Model xxx ranges:\n" << ComputeModelRanges(model)
//             << std::endl;
//
//   LOG(INFO) << "Model xxx ranges:\n" << ComputeModelRanges(model);
//
std::ostream& operator<<(std::ostream& out, const ModelRanges& ranges);

// Returns the ranges of the finite non-zero values in the given model.
ModelRanges ComputeModelRanges(const Model& model);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CPP_STATISTICS_H_
