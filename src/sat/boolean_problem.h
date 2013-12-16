// Copyright 2010-2013 Google
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
#ifndef OR_TOOLS_SAT_BOOLEAN_PROBLEM_H_
#define OR_TOOLS_SAT_BOOLEAN_PROBLEM_H_

#include "sat/boolean_problem.pb.h"
#include "sat/sat_solver.h"

namespace operations_research {
namespace sat {

// Loads a BooleanProblem into a given SatSolver instance.
bool LoadBooleanProblem(const LinearBooleanProblem& problem, SatSolver* solver);

// Adds the constraint that the objective is smaller or equals to the given
// upper bound.
bool AddObjectiveConstraint(const LinearBooleanProblem& problem,
                            bool use_lower_bound, Coefficient lower_bound,
                            bool use_upper_bound, Coefficient upper_bound,
                            SatSolver* solver);

// Returns the objective value under the current assignment.
Coefficient ComputeObjectiveValue(const LinearBooleanProblem& problem,
                                  const VariablesAssignment& assignment);

// Checks that an assignment is valid for the given BooleanProblem.
bool IsAssignmentValid(const LinearBooleanProblem& problem,
                       const VariablesAssignment& assignment);

// Converts a LinearBooleanProblem to the cnf file format.
// Note that this only works for pure SAT problems (only clauses), max-sat or
// weighted max-sat problems. Returns an empty std::string on error.
std::string LinearBooleanProblemToCnfString(const LinearBooleanProblem& problem);

// Store a variable assignment into the given BooleanAssignement proto.
// Note that only the assigned variables are stored, so the assignment may be
// incomplete.
void StoreAssignment(const VariablesAssignment& assignment,
                     BooleanAssignment* output);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_BOOLEAN_PROBLEM_H_
