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

#ifndef ORTOOLS_MATH_OPT_IO_LP_MODEL_UTILS_H_
#define ORTOOLS_MATH_OPT_IO_LP_MODEL_UTILS_H_

#include <string>

#include "absl/status/statusor.h"
#include "ortools/base/strong_vector.h"
#include "ortools/math_opt/io/lp/lp_model.h"

namespace operations_research::lp_format {

// Returns a copy of `model`, but where the variables appearing in no constraint
// have been deleted (and variable order is otherwise preserved).
//
// Note that because the variables are re-indexed, the constraints will have
// different values in `terms`.
LpModel RemoveUnusedVariables(const LpModel& model);

// Returns a copy of `model` where the variables are permuted by
// `new_index_to_old_index` (a permutation of the indices of the variables).
//
// Returns an error if `new_index_to_old_index` is not a valid permutation.
//
// Note that because the variables are re-indexed, the constraints will have
// different values in `terms`.
absl::StatusOr<LpModel> PermuteVariables(
    const LpModel& model,
    const util_intops::StrongVector<VariableIndex, VariableIndex>&
        new_index_to_old_index);

// Returns a copy of `model` where the variables are reordered by
// `order_by_name`, where `order_by_name` contains the name of each variable
// exactly one time, giving the new ordering.
//
// Returns an error if `order_by_name` does not contain the name each variable
// in the model exactly once.
//
// Note that because the variables are re-indexed, the constraints will have
// different values in `terms`.
absl::StatusOr<LpModel> PermuteVariables(
    const LpModel& model,
    const util_intops::StrongVector<VariableIndex, std::string>& order_by_name);

}  // namespace operations_research::lp_format

#endif  // ORTOOLS_MATH_OPT_IO_LP_MODEL_UTILS_H_
