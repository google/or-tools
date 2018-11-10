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

// Utility helpers for manipulating LinearProgram and other types defined in
// lp_data.

#ifndef OR_TOOLS_LP_DATA_LP_DATA_UTILS_H_
#define OR_TOOLS_LP_DATA_LP_DATA_UTILS_H_

#include "ortools/glop/parameters.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/matrix_scaler.h"

namespace operations_research {
namespace glop {

// For all constraints in linear_program, if the constraint has a slack
// variable, change its value in *values so that the constraints itself is
// satisfied.
// Note that this obviously won't always imply that the bounds of the slack
// variable itself will be satisfied.
// The code assumes (and DCHECKs) that all constraints with a slack variable
// have their upper and lower bounds both set to 0. This is ensured by
// LinearProgram::AddSlackVariablesWhereNecessary().
void ComputeSlackVariablesValues(const LinearProgram& linear_program,
                                 DenseRow* values);

// This is separated from LinearProgram class because of a cyclic dependency
// when scaling as an LP.
void Scale(LinearProgram* lp, SparseMatrixScaler* scaler,
           GlopParameters::ScalingAlgorithm scaling_method);

// A convenience method for above providing a default algorithm for callers that
// don't specify one.
void Scale(LinearProgram* lp, SparseMatrixScaler* scaler);

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_LP_DATA_UTILS_H_
