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

// This file contains some BopOptimizerBase implementations that are "complete"
// solvers. That is, they work on the full problem, and can solve the problem
// (and prove optimality) by themselves. Moreover, they can be run for short
// period of time and resumed later from the state they where left off.
//
// The idea is that it is worthwhile spending some time in these algorithms,
// because in some situation they can improve the current upper/lower bound or
// even solve the problem to optimality.
//
// Note(user): The GuidedSatFirstSolutionGenerator can also be used as a
// complete SAT solver provided that we keep running it after it has found a
// first solution. This is the default behavior of the kNotGuided policy.

#ifndef OR_TOOLS_BOP_COMPLETE_OPTIMIZER_H_
#define OR_TOOLS_BOP_COMPLETE_OPTIMIZER_H_

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "ortools/bop/bop_base.h"
#include "ortools/bop/bop_solution.h"
#include "ortools/bop/bop_types.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/encoding.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace bop {

// TODO(user): Merge this with the code in sat/optimization.cc
class SatCoreBasedOptimizer : public BopOptimizerBase {
 public:
  explicit SatCoreBasedOptimizer(const std::string& name);
  ~SatCoreBasedOptimizer() override;

 protected:
  bool ShouldBeRun(const ProblemState& problem_state) const override;
  Status Optimize(const BopParameters& parameters,
                  const ProblemState& problem_state, LearnedInfo* learned_info,
                  TimeLimit* time_limit) override;

 private:
  BopOptimizerBase::Status SynchronizeIfNeeded(
      const ProblemState& problem_state);
  sat::SatSolver::Status SolveWithAssumptions();

  int64_t state_update_stamp_;
  bool initialized_;
  bool assumptions_already_added_;
  sat::SatSolver solver_;
  sat::Coefficient offset_;
  sat::Coefficient lower_bound_;
  sat::Coefficient upper_bound_;
  sat::Coefficient stratified_lower_bound_;
  std::deque<sat::EncodingNode> repository_;
  std::vector<sat::EncodingNode*> nodes_;
};

}  // namespace bop
}  // namespace operations_research

#endif  // OR_TOOLS_BOP_COMPLETE_OPTIMIZER_H_
