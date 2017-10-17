// Copyright 2010-2017 Google
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
// Utilities used by frequency_assignment_problem.cc.
//

#ifndef OR_TOOLS_EXAMPLES_FAP_UTILITIES_H_
#define OR_TOOLS_EXAMPLES_FAP_UTILITIES_H_

#include <map>
#include <vector>

#include "ortools/constraint_solver/constraint_solver.h"
#include "examples/cpp/fap_parser.h"

namespace operations_research {

// Checks if the solution given from the Solver satisfies all
// the hard binary constraints specified in the ctr.txt.
bool CheckConstraintSatisfaction(
    const std::vector<FapConstraint>& data_constraints,
    const std::vector<int>& variables,
    const std::map<int, int>& index_from_key);

// Checks if the solution given from the Solver has not modified the values of
// the variables that were initially assigned and denoted as hard in var.txt.
bool CheckVariablePosition(const std::map<int, FapVariable>& data_variables,
                           const std::vector<int>& variables,
                           const std::map<int, int>& index_from_key);

// Counts the number of different values in the variable vector.
int NumberOfAssignedValues(const std::vector<int>& variables);

// Prints the duration of the solving process.
void PrintElapsedTime(const int64 time1, const int64 time2);

// Prints the solution found by the Hard Solver for feasible instances.
void PrintResultsHard(SolutionCollector* const collector,
                      const std::vector<IntVar*>& variables,
                      IntVar* const objective_var,
                      const std::map<int, FapVariable>& data_variables,
                      const std::vector<FapConstraint>& data_constraints,
                      const std::map<int, int>& index_from_key,
                      const std::vector<int>& key_from_index);

// Prints the solution found by the Soft Solver for unfeasible instances.
void PrintResultsSoft(SolutionCollector* const collector,
                      const std::vector<IntVar*>& variables,
                      IntVar* const total_cost,
                      const std::map<int, FapVariable>& hard_variables,
                      const std::vector<FapConstraint>& hard_constraints,
                      const std::map<int, FapVariable>& soft_variables,
                      const std::vector<FapConstraint>& soft_constraints,
                      const std::map<int, int>& index_from_key,
                      const std::vector<int>& key_from_index);

}  // namespace operations_research
#endif  // OR_TOOLS_EXAMPLES_FAP_UTILITIES_H_
