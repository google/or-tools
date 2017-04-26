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

#ifndef OR_TOOLS_BOP_BOP_FS_H_
#define OR_TOOLS_BOP_BOP_FS_H_

#include <string>

#include "ortools/base/basictypes.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/bop/bop_base.h"
#include "ortools/bop/bop_parameters.pb.h"
#include "ortools/bop/bop_solution.h"
#include "ortools/bop/bop_types.h"
#include "ortools/bop/bop_util.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace bop {

// Tries to find a first solution using SAT and a given assignment preference.
// This optimizer will never run again once it has found a solution except if
// the policy is kNotGuided in which case it will be ran again.
class GuidedSatFirstSolutionGenerator : public BopOptimizerBase {
 public:
  // The different guiding heuristics
  enum class Policy {
    kNotGuided,        // The default SAT solver.
    kLpGuided,         // Guided by the values of the linear relaxation.
    kObjectiveGuided,  // Guided by the objective coefficient.
    kUserGuided,       // Guided by the problem assignment_preference().
  };
  GuidedSatFirstSolutionGenerator(const std::string& name, Policy policy);
  ~GuidedSatFirstSolutionGenerator() override;

  bool ShouldBeRun(const ProblemState& problem_state) const override;

  // Note that if the last call to Optimize() returned CONTINUE and if the
  // problem didn't change, calling this will resume the solve from its last
  // position.
  Status Optimize(const BopParameters& parameters,
                  const ProblemState& problem_state, LearnedInfo* learned_info,
                  TimeLimit* time_limit) override;

 private:
  BopOptimizerBase::Status SynchronizeIfNeeded(
      const ProblemState& problem_state);

  const Policy policy_;
  bool abort_;
  int64 state_update_stamp_;
  std::unique_ptr<sat::SatSolver> sat_solver_;
};


// This class implements an optimizer that tries various random search
// strategies, each with a really low conflict limit. It can be used to generate
// a first solution or to improve an existing one.
//
// By opposition to all the other optimizers, this one doesn't return right away
// when a new solution is found. Instead, it continues to improve it as long as
// it has time.
//
// TODO(user): Coupled with some Local Search it might be used to diversify
//              the solutions. To try.
class BopRandomFirstSolutionGenerator : public BopOptimizerBase {
 public:
  BopRandomFirstSolutionGenerator(const std::string& name,
                                  const BopParameters& parameters,
                                  sat::SatSolver* sat_propagator,
                                  MTRandom* random);
  ~BopRandomFirstSolutionGenerator() override;

  bool ShouldBeRun(const ProblemState& problem_state) const override;
  Status Optimize(const BopParameters& parameters,
                  const ProblemState& problem_state, LearnedInfo* learned_info,
                  TimeLimit* time_limit) override;

 private:
  BopOptimizerBase::Status SynchronizeIfNeeded(
      const ProblemState& problem_state);

  int random_seed_;
  MTRandom* random_;
  sat::SatSolver* sat_propagator_;
  uint32 sat_seed_;
};

// This class computes the linear relaxation of the state problem.
// Optimize() fills the relaxed values (possibly floating values) that can be
// used by other optimizers as BopSatLpFirstSolutionGenerator for instance,
// and the lower bound.
class LinearRelaxation : public BopOptimizerBase {
 public:
  LinearRelaxation(const BopParameters& parameters, const std::string& name);
  ~LinearRelaxation() override;

  bool ShouldBeRun(const ProblemState& problem_state) const override;
  Status Optimize(const BopParameters& parameters,
                  const ProblemState& problem_state, LearnedInfo* learned_info,
                  TimeLimit* time_limit) override;

 private:
  BopOptimizerBase::Status SynchronizeIfNeeded(
      const ProblemState& problem_state);

  // Runs Glop to solve the current lp_model_.
  // Updates the time limit and returns the status of the solve.
  // Note that when the solve is incremental, the preprocessor is deactivated,
  // and the dual simplex is used.
  glop::ProblemStatus Solve(bool incremental_solve, TimeLimit* time_limit);

  // Computes and returns a better best bound using strong branching, i.e.
  // doing a what-if analysis on each variable v: compute the best bound when
  // v is assigned to true, compute the best bound when v is assigned to false,
  // and then use those best bounds to improve the overall best bound.
  // As a side effect, it might fix some variables.
  double ComputeLowerBoundUsingStrongBranching(LearnedInfo* learned_info,
                                               TimeLimit* time_limit);

  // Returns true when the cost is worse than the cost of the current solution.
  // If they are within the given tolerance, returns false.
  bool CostIsWorseThanSolution(double scaled_cost, double tolerance) const;

  const BopParameters parameters_;
  int64 state_update_stamp_;
  bool lp_model_loaded_;
  glop::LinearProgram lp_model_;
  glop::LPSolver lp_solver_;
  double scaling_;
  double offset_;
  int num_fixed_variables_;
  bool problem_already_solved_;
  double scaled_solution_cost_;
};

}  // namespace bop
}  // namespace operations_research
#endif  // OR_TOOLS_BOP_BOP_FS_H_
