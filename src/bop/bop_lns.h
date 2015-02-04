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

#ifndef OR_TOOLS_BOP_BOP_LNS_H_
#define OR_TOOLS_BOP_BOP_LNS_H_

#include <string>
#include <vector>

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
#include "util/stats.h"
#include "util/time_limit.h"

namespace operations_research {
namespace bop {
// TODO(user): Remove this class.
class SatPropagator {
 public:
  explicit SatPropagator(const LinearBooleanProblem& problem);

  bool LoadBooleanProblem();
  bool OverConstrainObjective(const BopSolution& current_solution);

  void AddPropagationRelation(sat::Literal decision_literal,
                              VariableIndex var_id);

  void AddSymmetries(std::vector<std::unique_ptr<SparsePermutation>>* generators);
  sat::SatSolver* GetSatSolver();

  const std::vector<sat::Literal>& propagated_by(VariableIndex var_id) const {
    return propagated_by_[var_id];
  }

 private:
  const LinearBooleanProblem& problem_;
  std::unique_ptr<sat::SatSolver> sat_solver_;
  int64 solution_cost_;
  std::vector<std::unique_ptr<SparsePermutation>> symmetries_generators_;
  ITIVector<VariableIndex, std::vector<sat::Literal> > propagated_by_;
};

class BopCompleteLNSOptimizer : public BopOptimizerBase {
 public:
  explicit BopCompleteLNSOptimizer(const std::string& name,
                                   const BopConstraintTerms& objective_terms);

  virtual ~BopCompleteLNSOptimizer();

 protected:
  virtual bool RunOncePerSolution() const { return true; }
  virtual bool NeedAFeasibleSolution() const { return true; }
  virtual Status Optimize(const BopParameters& parameters,
                          const ProblemState& problem_state,
                          LearnedInfo* learned_info, TimeLimit* time_limit);

  BopOptimizerBase::Status SynchronizeIfNeeded(
      const ProblemState& problem_state);

  int64 state_update_stamp_;
  LinearBooleanProblem problem_;
  std::unique_ptr<BopSolution> initial_solution_;
  const BopConstraintTerms& objective_terms_;
};

class BopRandomLNSOptimizer : public BopOptimizerBase {
 public:
  explicit BopRandomLNSOptimizer(const std::string& name,
                                 const BopConstraintTerms& objective_terms,
                                 int random_seed,
                                 bool use_sat_to_choose_lns_neighbourhood,
                                 SatPropagator* sat_propagator);
  virtual ~BopRandomLNSOptimizer();

 private:
  virtual bool RunOncePerSolution() const { return false; }
  virtual bool NeedAFeasibleSolution() const { return true; }
  virtual Status Optimize(const BopParameters& parameters,
                          const ProblemState& problem_state,
                          LearnedInfo* learned_info, TimeLimit* time_limit);
  Status GenerateProblemUsingSat(const BopSolution& initial_solution,
                                 double target_difficulty,
                                 TimeLimit* time_limit,
                                 BopParameters* parameters,
                                 BopSolution* solution,
                                 std::vector<sat::Literal>* fixed_variables);
  Status GenerateProblem(const BopSolution& initial_solution,
                         double target_difficulty, TimeLimit* time_limit,
                         std::vector<sat::Literal>* fixed_variables);

  void RelaxVariable(VariableIndex var_id, const BopSolution& solution);

  BopOptimizerBase::Status SynchronizeIfNeeded(
      const ProblemState& problem_state);

  int64 state_update_stamp_;
  const LinearBooleanProblem* problem_;
  std::unique_ptr<BopSolution> initial_solution_;
  const bool use_sat_to_choose_lns_neighbourhood_;
  SatPropagator* const sat_propagator_;
  const BopConstraintTerms& objective_terms_;
  MTRandom random_;

  // RandomLNS parameter whose values are kept from one run to the next.
  std::vector<sat::Literal> literals_;
  std::vector<bool> is_in_literals_;
  LubyAdaptiveParameterValue adaptive_difficulty_;

  SparseBitset<VariableIndex> to_relax_;
};

class BopRandomConstraintLNSOptimizer : public BopOptimizerBase {
 public:
  explicit BopRandomConstraintLNSOptimizer(
      const std::string& name, const BopConstraintTerms& objective_terms,
      int random_seed);
  virtual ~BopRandomConstraintLNSOptimizer();

 private:
  virtual bool RunOncePerSolution() const { return false; }
  virtual bool NeedAFeasibleSolution() const { return true; }
  virtual Status Optimize(const BopParameters& parameters,
                          const ProblemState& problem_state,
                          LearnedInfo* learned_info, TimeLimit* time_limit);

  BopOptimizerBase::Status SynchronizeIfNeeded(
      const ProblemState& problem_state);

  int64 state_update_stamp_;
  const LinearBooleanProblem* problem_;
  std::unique_ptr<BopSolution> initial_solution_;
  const BopConstraintTerms& objective_terms_;
  MTRandom random_;
  SparseBitset<VariableIndex> to_relax_;
};
}  // namespace bop
}  // namespace operations_research
#endif  // OR_TOOLS_BOP_BOP_LNS_H_
