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

// Utilities used by frequency_assignment_problem.cc.

#ifndef OR_TOOLS_EXAMPLES_FAP_UTILITIES_H_
#define OR_TOOLS_EXAMPLES_FAP_UTILITIES_H_

#include <cstdint>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/types/span.h"
#include "examples/cpp/fap_parser.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

// Checks if the solution given from the Solver satisfies all
// the hard binary constraints specified in the ctr.txt.
bool CheckConstraintSatisfaction(
    absl::Span<const FapConstraint> data_constraints,
    absl::Span<const int> variables,
    const absl::btree_map<int, int>& index_from_key);

// Checks if the solution given from the Solver has not modified the values of
// the variables that were initially assigned and denoted as hard in var.txt.
bool CheckVariablePosition(
    const absl::btree_map<int, FapVariable>& data_variables,
    absl::Span<const int> variables,
    const absl::btree_map<int, int>& index_from_key);

// Counts the number of different values in the variable vector.
int NumberOfAssignedValues(absl::Span<const int> variables);

// Prints the duration of the solving process.
void PrintElapsedTime(int64_t time1, int64_t time2);

// Prints the solution found by the Hard Solver for feasible instances.
void PrintResultsHard(SolutionCollector* collector,
                      const std::vector<IntVar*>& variables,
                      IntVar* objective_var,
                      const absl::btree_map<int, FapVariable>& data_variables,
                      absl::Span<const FapConstraint> data_constraints,
                      const absl::btree_map<int, int>& index_from_key,
                      absl::Span<const int> key_from_index);

// Prints the solution found by the Soft Solver for unfeasible instances.
void PrintResultsSoft(SolutionCollector* collector,
                      const std::vector<IntVar*>& variables, IntVar* total_cost,
                      const absl::btree_map<int, FapVariable>& hard_variables,
                      absl::Span<const FapConstraint> hard_constraints,
                      const absl::btree_map<int, FapVariable>& soft_variables,
                      absl::Span<const FapConstraint> soft_constraints,
                      const absl::btree_map<int, int>& index_from_key,
                      absl::Span<const int> key_from_index);

}  // namespace operations_research
#endif  // OR_TOOLS_EXAMPLES_FAP_UTILITIES_H_
