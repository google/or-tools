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

#include "base/basictypes.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/int_type_indexed_vector.h"
#include "base/int_type.h"
#include "bop/bop_base.h"
#include "bop/bop_parameters.pb.h"
#include "bop/bop_solution.h"
#include "bop/bop_types.h"
#include "bop/bop_util.h"
#include "glop/lp_solver.h"
#include "sat/boolean_problem.pb.h"
#include "sat/sat_solver.h"
#include "util/time_limit.h"

namespace operations_research {
namespace bop {
class BopSatObjectiveFirstSolutionGenerator : public BopOptimizerBase {
 public:
  BopSatObjectiveFirstSolutionGenerator(const std::string& name,
                                        double time_limit_ratio);
  virtual ~BopSatObjectiveFirstSolutionGenerator();

  virtual bool RunOncePerSolution() const { return false; }
  virtual bool NeedAFeasibleSolution() const { return false; }
  virtual Status Synchronize(const ProblemState& problem_state);
  virtual Status Optimize(const BopParameters& parameters,
                          LearnedInfo* learned_info, TimeLimit* time_limit);

 private:
  double time_limit_ratio_;
  bool first_solve_;
  sat::SatSolver sat_solver_;
  int64 lower_bound_;
  int64 upper_bound_;
  bool problem_already_solved_;
};

class BopSatLpFirstSolutionGenerator : public BopOptimizerBase {
 public:
  BopSatLpFirstSolutionGenerator(const std::string& name, double time_limit_ratio);
  virtual ~BopSatLpFirstSolutionGenerator();

  virtual bool RunOncePerSolution() const { return false; }
  virtual bool NeedAFeasibleSolution() const { return false; }
  virtual Status Synchronize(const ProblemState& problem_state);
  virtual Status Optimize(const BopParameters& parameters,
                          LearnedInfo* learned_info, TimeLimit* time_limit);

 private:
  double time_limit_ratio_;
  bool first_solve_;
  sat::SatSolver sat_solver_;
  int64 lower_bound_;
  int64 upper_bound_;
  glop::DenseRow lp_values_;
};


// This class implements an optimizer that looks for a better solution than the
// initial solution, if any, by randomly generating new solutions using the
// SAT solver.
// It can be used to both generate a first solution and improve an existing one.
// TODO(user): Coupled with some Local Search it might be use to diversify
//              the solutions. To try.
class BopRandomFirstSolutionGenerator : public BopOptimizerBase {
 public:
  BopRandomFirstSolutionGenerator(int random_seed, const std::string& name,
                                  double time_limit_ratio);
  virtual ~BopRandomFirstSolutionGenerator();

  virtual bool RunOncePerSolution() const { return false; }
  virtual bool NeedAFeasibleSolution() const { return false; }
  virtual Status Synchronize(const ProblemState& problem_state);
  virtual Status Optimize(const BopParameters& parameters,
                          LearnedInfo* learned_info, TimeLimit* time_limit);

 private:
  double time_limit_ratio_;
  const LinearBooleanProblem* problem_;
  std::unique_ptr<BopSolution> initial_solution_;
  glop::DenseRow lp_values_;
  MTRandom random_;
  sat::SatSolver sat_solver_;
  uint32 sat_seed_;
  bool first_solve_;
};

// This class computes the linear relaxation of the state problem.
// Optimize() fills the relaxed values (possibly floating values) that can be
// used by other optimizers as BopSatLpFirstSolutionGenerator for instance,
// and the lower bound.
class LinearRelaxation : public BopOptimizerBase {
 public:
  LinearRelaxation(const BopParameters& parameters, const std::string& name,
                   double time_limit_ratio);
  virtual ~LinearRelaxation();

  virtual bool RunOncePerSolution() const { return false; }
  virtual bool NeedAFeasibleSolution() const { return false; }
  virtual Status Synchronize(const ProblemState& problem_state);
  virtual Status Optimize(const BopParameters& parameters,
                          LearnedInfo* learned_info, TimeLimit* time_limit);

 private:
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
  const double time_limit_ratio_;
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
