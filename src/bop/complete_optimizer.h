// Copyright 2010-2014 Google
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

#ifndef OR_TOOLS_BOP_COMPLETE_OPTIMIZER_H_
#define OR_TOOLS_BOP_COMPLETE_OPTIMIZER_H_

#include "bop/bop_base.h"
#include "bop/bop_solution.h"
#include "bop/bop_types.h"
#include "sat/encoding.h"
#include "sat/sat_solver.h"
#include "sat/boolean_problem.pb.h"

namespace operations_research {
namespace bop {

class SatLinearScanOptimizer : public BopOptimizerBase {
 public:
  // TODO(user): change api to deal with error when loading the problem?
  explicit SatLinearScanOptimizer(const std::string& name);
  virtual ~SatLinearScanOptimizer();

 protected:
  virtual bool RunOncePerSolution() const { return false; }
  // TODO(user): The scan optimizer doesn't need a solution.
  virtual bool NeedAFeasibleSolution() const { return true; }
  virtual Status Synchronize(const ProblemState& problem_state);
  virtual Status Optimize(const BopParameters& parameters,
                          LearnedInfo* learned_info, TimeLimit* time_limit);

 private:
  int64 initial_solution_cost_;
  sat::SatSolver solver_;
};

// TODO(user): Merge this with the code in sat/optimization.cc
class SatCoreBasedOptimizer : public BopOptimizerBase {
 public:
  explicit SatCoreBasedOptimizer(const std::string& name);
  virtual ~SatCoreBasedOptimizer();

 protected:
  virtual bool RunOncePerSolution() const { return false; }
  // TODO(user): The core based optimizer doesn't need a solution.
  virtual bool NeedAFeasibleSolution() const { return true; }
  virtual Status Synchronize(const ProblemState& problem_state);
  virtual Status Optimize(const BopParameters& parameters,
                          LearnedInfo* learned_info, TimeLimit* time_limit);

 private:
  bool initialized_;
  std::unique_ptr<BopSolution> initial_solution_;
  sat::SatSolver::Status SolveWithAssumptions();
  bool assumptions_already_added_;
  sat::SatSolver solver_;
  sat::Coefficient offset_;
  sat::Coefficient lower_bound_;
  sat::Coefficient upper_bound_;
  sat::Coefficient stratified_lower_bound_;
  std::vector<std::unique_ptr<sat::EncodingNode>> repository_;
  std::vector<sat::EncodingNode*> nodes_;
};

}  // namespace bop
}  // namespace operations_research
#endif  // OR_TOOLS_BOP_COMPLETE_OPTIMIZER_H_
