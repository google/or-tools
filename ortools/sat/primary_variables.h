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

#ifndef OR_TOOLS_SAT_PRIMARY_VARIABLES_H_
#define OR_TOOLS_SAT_PRIMARY_VARIABLES_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "ortools/sat/cp_model.pb.h"

namespace operations_research {
namespace sat {

// This structs defines a way of splitting the variables of the model in two
// groups: primary variables and secondary variables. Those are specified so
// that the value of secondary_variables[i] is uniquely fixed by applying the
// constraint dependency_resolution_constraint_index[i] to the values of the
// primary variables and the values of the variables in the set
// {secondary_variables[0], ..., secondary_variables[i-1]}.
//
// The set of primary variables is implicitly defined by the set of variables
// that are not in secondary_variables.
//
// A useful property of this structure is that given an assignment of primary
// variables that corresponds to a feasible solution, we can deduce all the
// values of the secondary variables. Note that if the values of the primary
// variables are unfeasible, then it might not be possible to deduce the values
// of the secondary variables.
struct VariableRelationships {
  std::vector<int> secondary_variables;
  std::vector<ConstraintProto> dependency_resolution_constraint;

  // A pair of(x, y) means that one needs to compute the value of y before
  // computing the value of x. This defines an implicit dependency DAG for
  // computing the secondary variables from the primary.
  std::vector<std::pair<int, int>> variable_dependencies;

  // The list of model constraints that are redundant (ie., satisfied by
  // construction) when the secondary variables are computed from the primary
  // ones. In other words, a model has a solution for a set of primary
  // variables {x_i} if and only if all the variable bounds and non-redundant
  // constraints are satisfied after the secondary variables have been computed
  // from the primary ones.
  std::vector<int> redundant_constraint_indices;
};

// Compute the variable relationships for a given model. Note that there are
// multiple possible ways variables can be split in primary and secondary, so
// this function will use an heuristic to try to find as many secondary
// variables as possible. This runs in linear time in the model size (ie., the
// sum of the number of variables over the constraints).
VariableRelationships ComputeVariableRelationships(const CpModelProto& model);

// Given a vector with a partial solution where only the primary variables have
// a correct value, this function will overwrite the values of the secondary
// variables so that the solution is complete and valid.
bool ComputeAllVariablesFromPrimaryVariables(
    const CpModelProto& model, const VariableRelationships& relationships,
    std::vector<int64_t>* solution);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PRIMARY_VARIABLES_H_
