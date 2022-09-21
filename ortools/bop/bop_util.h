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

#ifndef OR_TOOLS_BOP_BOP_UTIL_H_
#define OR_TOOLS_BOP_BOP_UTIL_H_

#include <vector>

#include "ortools/base/basictypes.h"
#include "ortools/base/integral_types.h"
#include "ortools/bop/bop_base.h"
#include "ortools/bop/bop_solution.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace bop {
// Loads the problem state into the sat_solver. If the problem has already been
// loaded in the sat_solver, fixed variables and objective bounds are updated.
// Returns the status of the load:
//   - CONTINUE: State problem successfully loaded.
//   - OPTIMAL_SOLUTION_FOUND: Solution is proved optimal.
//     If a feasible solution exists, this load function imposes the solution
//     to be strictly better. Then when SAT proves the problem is UNSAT, that
//     actually means that the current solution is optimal.
//   - INFEASIBLE: The problem is proved to be infeasible.
// Note that the sat_solver will be backtracked to the root level in order
// to add new constraints.
BopOptimizerBase::Status LoadStateProblemToSatSolver(
    const ProblemState& problem_state, sat::SatSolver* sat_solver);

// Extracts from the sat solver any new information about the problem. Note that
// the solver is not const because this function clears what is considered
// "new".
void ExtractLearnedInfoFromSatSolver(sat::SatSolver* solver, LearnedInfo* info);

void SatAssignmentToBopSolution(const sat::VariablesAssignment& assignment,
                                BopSolution* solution);

class AdaptiveParameterValue {
 public:
  // Initial value is in [0..1].
  explicit AdaptiveParameterValue(double initial_value);

  void Reset();
  void Increase();
  void Decrease();

  double value() const { return value_; }

 private:
  double value_;
  int num_changes_;
};

class LubyAdaptiveParameterValue {
 public:
  // Initial value is in [0..1].
  explicit LubyAdaptiveParameterValue(double initial_value);

  void Reset();

  void IncreaseParameter();
  void DecreaseParameter();

  double GetParameterValue() const;

  void UpdateLuby();
  bool BoostLuby();
  int luby_value() const { return luby_value_; }

 private:
  int luby_id_;
  int luby_boost_;
  int luby_value_;

  std::vector<AdaptiveParameterValue> difficulties_;
};
}  // namespace bop
}  // namespace operations_research
#endif  // OR_TOOLS_BOP_BOP_UTIL_H_
