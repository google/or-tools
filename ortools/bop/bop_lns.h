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

#ifndef OR_TOOLS_BOP_BOP_LNS_H_
#define OR_TOOLS_BOP_BOP_LNS_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ortools/base/basictypes.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/strong_vector.h"
#include "ortools/bop/bop_base.h"
#include "ortools/bop/bop_parameters.pb.h"
#include "ortools/bop/bop_solution.h"
#include "ortools/bop/bop_types.h"
#include "ortools/bop/bop_util.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/stats.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace bop {

// Uses SAT to solve the full problem under the constraint that the new solution
// should be under a given Hamming distance of the current solution.
class BopCompleteLNSOptimizer : public BopOptimizerBase {
 public:
  BopCompleteLNSOptimizer(const std::string& name,
                          const BopConstraintTerms& objective_terms);
  ~BopCompleteLNSOptimizer() final;

 private:
  bool ShouldBeRun(const ProblemState& problem_state) const final;
  Status Optimize(const BopParameters& parameters,
                  const ProblemState& problem_state, LearnedInfo* learned_info,
                  TimeLimit* time_limit) final;

  BopOptimizerBase::Status SynchronizeIfNeeded(
      const ProblemState& problem_state, int num_relaxed_vars);

  int64_t state_update_stamp_;
  std::unique_ptr<sat::SatSolver> sat_solver_;
  const BopConstraintTerms& objective_terms_;
};

// Interface of the different LNS neighborhood generation algorithm.
//
// NOTE(user): Using a sat_propagator as the output of the algorithm allows for
// a really simple and efficient interface for the generator that relies on it.
// However, if a generator doesn't rely on it at all, it may slow down a bit the
// code (to investigate). If this happens, we will probably need another
// function here and a way to select between which one to call.
class NeighborhoodGenerator {
 public:
  NeighborhoodGenerator() {}
  virtual ~NeighborhoodGenerator() {}

  // Interface for the neighborhood generation.
  //
  // The difficulty will be in [0, 1] and is related to the asked neighborhood
  // size (and thus local problem difficulty). A difficulty of 0.0 means empty
  // neighborhood and a difficulty of 1.0 means the full problem. The algorithm
  // should try to generate an neighborhood according to this difficulty, which
  // will be dynamically adjusted depending on whether or not we can solve the
  // subproblem.
  //
  // The given sat_propagator will be reset and then configured so that all the
  // variables propagated on its trail should be fixed. That is, the
  // neighborhood will correspond to the unassigned variables in the
  // sat_propagator. Note that sat_propagator_.IsModelUnsat() will be checked
  // after this is called so it is okay to abort if this happens.
  //
  // Preconditions:
  // - The given sat_propagator_ should have the current problem loaded (with
  //   the constraint to find a better solution that any current solution).
  // - The problem state must contains a feasible solution.
  virtual void GenerateNeighborhood(const ProblemState& problem_state,
                                    double difficulty,
                                    sat::SatSolver* sat_propagator) = 0;
};

// A generic LNS optimizer which generates neighborhoods according to the given
// NeighborhoodGenerator and automatically adapt the neighborhood size depending
// on how easy it is to solve the associated problem.
class BopAdaptiveLNSOptimizer : public BopOptimizerBase {
 public:
  // Takes ownership of the given neighborhood_generator.
  // The sat_propagator is assumed to contains the current problem.
  BopAdaptiveLNSOptimizer(const std::string& name, bool use_lp_to_guide_sat,
                          NeighborhoodGenerator* neighborhood_generator,
                          sat::SatSolver* sat_propagator);
  ~BopAdaptiveLNSOptimizer() final;

 private:
  bool ShouldBeRun(const ProblemState& problem_state) const final;
  Status Optimize(const BopParameters& parameters,
                  const ProblemState& problem_state, LearnedInfo* learned_info,
                  TimeLimit* time_limit) final;

  const bool use_lp_to_guide_sat_;
  std::unique_ptr<NeighborhoodGenerator> neighborhood_generator_;
  sat::SatSolver* const sat_propagator_;

  // Adaptive neighborhood size logic.
  // The values are kept from one run to the next.
  LubyAdaptiveParameterValue adaptive_difficulty_;
};

// Generates a neighborhood by randomly fixing a subset of the objective
// variables that are currently at their lower cost.
class ObjectiveBasedNeighborhood : public NeighborhoodGenerator {
 public:
  ObjectiveBasedNeighborhood(const BopConstraintTerms* objective_terms,
                             absl::BitGenRef random)
      : objective_terms_(*objective_terms), random_(random) {}
  ~ObjectiveBasedNeighborhood() final {}

 private:
  void GenerateNeighborhood(const ProblemState& problem_state,
                            double difficulty,
                            sat::SatSolver* sat_propagator) final;
  const BopConstraintTerms& objective_terms_;
  absl::BitGenRef random_;
};

// Generates a neighborhood by randomly selecting a subset of constraints and
// fixing the objective variables that are currently at their lower cost and
// not in the given subset of constraints.
class ConstraintBasedNeighborhood : public NeighborhoodGenerator {
 public:
  ConstraintBasedNeighborhood(const BopConstraintTerms* objective_terms,
                              absl::BitGenRef random)
      : objective_terms_(*objective_terms), random_(random) {}
  ~ConstraintBasedNeighborhood() final {}

 private:
  void GenerateNeighborhood(const ProblemState& problem_state,
                            double difficulty,
                            sat::SatSolver* sat_propagator) final;
  const BopConstraintTerms& objective_terms_;
  absl::BitGenRef random_;
};

// Generates a neighborhood by taking a random local neighborhood in an
// undirected graph where the nodes are the variables and two nodes are linked
// if they appear in the same constraint.
class RelationGraphBasedNeighborhood : public NeighborhoodGenerator {
 public:
  RelationGraphBasedNeighborhood(const sat::LinearBooleanProblem& problem,
                                 absl::BitGenRef random);
  ~RelationGraphBasedNeighborhood() final {}

 private:
  void GenerateNeighborhood(const ProblemState& problem_state,
                            double difficulty,
                            sat::SatSolver* sat_propagator) final;

  // TODO(user): reuse by_variable_matrix_ from the LS? Note however than we
  // don't need the coefficients here.
  absl::StrongVector<VariableIndex, std::vector<ConstraintIndex>> columns_;
  absl::BitGenRef random_;
};

}  // namespace bop
}  // namespace operations_research
#endif  // OR_TOOLS_BOP_BOP_LNS_H_
