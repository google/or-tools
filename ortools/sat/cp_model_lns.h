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

#ifndef OR_TOOLS_SAT_CP_MODEL_LNS_H_
#define OR_TOOLS_SAT_CP_MODEL_LNS_H_

#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_presolve.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/subsolver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/adaptative_parameter_value.h"
#include "ortools/util/logging.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Neighborhood returned by Neighborhood generators.
struct Neighborhood {
  // True if neighborhood generator was able to generate a neighborhood.
  bool is_generated = false;

  // True if an optimal solution to the neighborhood is also an optimal solution
  // to the original model.
  bool is_reduced = false;

  // True if this neighborhood was just obtained by fixing some variables.
  bool is_simple = false;

  // Specification of the delta between the initial model and the lns fragment.
  // The delta will contains all variables from the initial model, potentially
  // with updated domains.
  // It can contains new variables and new constraints, and solution hinting.
  CpModelProto delta;
  std::vector<int> constraints_to_ignore;

  // Neighborhood Id. Used to identify the neighborhood by a generator.
  // Currently only used by WeightedRandomRelaxationNeighborhoodGenerator.
  // TODO(user): Make sure that the id is unique for each generated
  // neighborhood for each generator.
  int64_t id = 0;

  // Used for identifying the source of the neighborhood if it is generated
  // using solution repositories.
  std::string source_info = "";

  // Statistic, only filled when is_simple is true.
  int num_relaxed_variables = 0;
  int num_relaxed_variables_in_objective = 0;

  // Only filled when is_simple is true. If we solve the fragment to optimality,
  // then we can just fix the variable listed here to that optimal solution.
  //
  // This can happen if the neighborhood fully cover some part that are
  // completely independent from the rest of the model. Like for instance an
  // unused but not yet fixed variable.
  //
  // WARNING: all such variables should be fixed at once in a lock-like manner,
  // because they can be multiple optimal solutions on these variables.
  std::vector<int> variables_that_can_be_fixed_to_local_optimum;
};

// Contains pre-computed information about a given CpModelProto that is meant
// to be used to generate LNS neighborhood. This class can be shared between
// more than one generator in order to reduce memory usage.
//
// Note that its implement the SubSolver interface to be able to Synchronize()
// the bounds of the base problem with the external world.
class NeighborhoodGeneratorHelper : public SubSolver {
 public:
  NeighborhoodGeneratorHelper(CpModelProto const* model_proto,
                              SatParameters const* parameters,
                              SharedResponseManager* shared_response,
                              SharedBoundsManager* shared_bounds = nullptr);

  // SubSolver interface.
  bool TaskIsAvailable() override { return false; }
  std::function<void()> GenerateTask(int64_t /*task_id*/) override {
    return {};
  }
  void Synchronize() override;

  // Returns the LNS fragment where the given variables are fixed to the value
  // they take in the given solution.
  Neighborhood FixGivenVariables(
      const CpSolverResponse& base_solution,
      const absl::flat_hash_set<int>& variables_to_fix) const;

  // Returns the neighborhood where the given constraints are removed.
  Neighborhood RemoveMarkedConstraints(
      const std::vector<int>& constraints_to_remove) const;

  // Returns the LNS fragment which will relax all inactive variables and all
  // variables in relaxed_variables.
  Neighborhood RelaxGivenVariables(
      const CpSolverResponse& initial_solution,
      const std::vector<int>& relaxed_variables) const;

  // Returns a trivial model by fixing all active variables to the initial
  // solution values.
  Neighborhood FixAllVariables(const CpSolverResponse& initial_solution) const;

  // Returns a neighborhood that correspond to the full problem.
  Neighborhood FullNeighborhood() const;

  // Returns a neighborhood that will just be skipped.
  // It usually indicate that the generator failed to generated a neighborhood.
  Neighborhood NoNeighborhood() const;

  // Adds solution hinting to the neighborhood from the value of the initial
  // solution.
  void AddSolutionHinting(const CpSolverResponse& initial_solution,
                          CpModelProto* model_proto) const;

  // Indicates if the variable can be frozen. It happens if the variable is non
  // constant, and if it is a decision variable, or if
  // focus_on_decision_variables is false.
  bool IsActive(int var) const ABSL_SHARED_LOCKS_REQUIRED(graph_mutex_);

  // Returns the list of "active" variables.
  std::vector<int> ActiveVariables() const {
    std::vector<int> result;
    absl::ReaderMutexLock lock(&graph_mutex_);
    result = active_variables_;
    return result;
  }

  int NumActiveVariables() const {
    absl::ReaderMutexLock lock(&graph_mutex_);
    return active_variables_.size();
  }

  std::vector<int> ActiveObjectiveVariables() const {
    std::vector<int> result;
    absl::ReaderMutexLock lock(&graph_mutex_);
    result = active_objective_variables_;
    return result;
  }

  bool DifficultyMeansFullNeighborhood(double difficulty) const {
    absl::ReaderMutexLock lock(&graph_mutex_);
    const int target_size = std::ceil(difficulty * active_variables_.size());
    return target_size == active_variables_.size();
  }

  // Returns the vector of active variables. The graph_mutex_ must be
  // locked before calling this method.
  const std::vector<int>& ActiveVariablesWhileHoldingLock() const
      ABSL_SHARED_LOCKS_REQUIRED(graph_mutex_) {
    return active_variables_;
  }

  // Constraints <-> Variables graph.
  // Note that only non-constant variable are listed here.
  const std::vector<std::vector<int>>& ConstraintToVar() const
      ABSL_SHARED_LOCKS_REQUIRED(graph_mutex_) {
    return constraint_to_var_;
  }
  const std::vector<std::vector<int>>& VarToConstraint() const
      ABSL_SHARED_LOCKS_REQUIRED(graph_mutex_) {
    return var_to_constraint_;
  }

  // Returns all the constraints indices of a given type.
  const absl::Span<const int> TypeToConstraints(
      ConstraintProto::ConstraintCase type) const {
    if (type >= type_to_constraints_.size()) return {};
    return absl::MakeSpan(type_to_constraints_[type]);
  }

  // Returns the list of indices of active interval constraints according
  // to the initial_solution and the parameter lns_focus_on_performed_intervals.
  // If true, this method returns the list of performed intervals in the
  // solution. If false, it returns all intervals of the model.
  std::vector<int> GetActiveIntervals(
      const CpSolverResponse& initial_solution) const;

  // Returns the set of unique intervals list appearing in a no_overlap,
  // cumulative, or as a dimension of a no_overlap_2d constraint.
  std::vector<std::vector<int>> GetUniqueIntervalSets() const;

  // Returns one sub-vector per circuit or per single vehicle ciruit in a routes
  // constraints. Each circuit is non empty, and does not contain any
  // self-looping arcs. Path are sorted, starting from the arc with the lowest
  // tail index, and going in sequence up to the last arc before the circuit is
  // closed. Each entry correspond to the arc literal on the circuit.
  std::vector<std::vector<int>> GetRoutingPaths(
      const CpSolverResponse& initial_solution) const;

  // Returns all precedences extracted from the scheduling constraint and the
  // initial solution. The precedences will be sorted by the natural order
  // the pairs of integers.
  std::vector<std::pair<int, int>> GetSchedulingPrecedences(
      const absl::flat_hash_set<int>& ignored_intervals,
      const CpSolverResponse& initial_solution, absl::BitGenRef random) const;

  // The initial problem.
  // Note that the domain of the variables are not updated here.
  const CpModelProto& ModelProto() const { return model_proto_; }
  const SatParameters& Parameters() const { return parameters_; }

  const SharedResponseManager& shared_response() const {
    return *shared_response_;
  }

  // TODO(user): Refactor the class to be thread-safe instead, it should be
  // safer and more easily maintenable. Some complication with accessing the
  // variable<->constraint graph efficiently though.

  // Note: This mutex needs to be public for thread annotations.
  mutable absl::Mutex graph_mutex_;

  // TODO(user): Display LNS statistics through the StatisticsString()
  // method.

 private:
  // Precompute stuff that will never change. During the execution, only the
  // domain of the variable will change, so data that only depends on the
  // constraints need to be computed just once.
  void InitializeHelperData();

  // Recompute most of the class member. This needs to be called when the
  // domains of the variables are updated.
  void RecomputeHelperData();

  // Indicates if a variable is fixed in the model.
  bool IsConstant(int var) const ABSL_SHARED_LOCKS_REQUIRED(domain_mutex_);

  // Returns true if the domain on the objective is constraining and we might
  // get a lower objective value at optimum without it.
  bool ObjectiveDomainIsConstraining() const
      ABSL_SHARED_LOCKS_REQUIRED(domain_mutex_);

  const SatParameters& parameters_;
  const CpModelProto& model_proto_;
  int shared_bounds_id_;
  SharedBoundsManager* shared_bounds_;
  SharedResponseManager* shared_response_;

  // This proto will only contain the field variables() with an updated version
  // of the domains compared to model_proto_.variables(). We do it like this to
  // reduce the memory footprint of the helper when the model is large.
  //
  // TODO(user): Use custom domain repository rather than a proto?
  CpModelProto model_proto_with_only_variables_ ABSL_GUARDED_BY(domain_mutex_);

  // Constraints by types. This never changes.
  std::vector<std::vector<int>> type_to_constraints_;

  // Whether a model_proto_ variable appear in the objective. This never
  // changes.
  std::vector<bool> is_in_objective_;

  // A copy of CpModelProto where we did some basic presolving to remove all
  // constraint that are always true. The Variable-Constraint graph is based on
  // this model. Note that only the constraints field is present here.
  CpModelProto simplied_model_proto_ ABSL_GUARDED_BY(graph_mutex_);

  // Variable-Constraint graph.
  // We replace an interval by its variables in the scheduling constraints.
  //
  // TODO(user): Note that the objective is not considered here. Which is fine
  // except if the objective domain is constraining.
  std::vector<std::vector<int>> constraint_to_var_
      ABSL_GUARDED_BY(graph_mutex_);
  std::vector<std::vector<int>> var_to_constraint_
      ABSL_GUARDED_BY(graph_mutex_);

  // Connected components of the variable-constraint graph. If a variable is
  // constant, it will not appear in any component and
  // var_to_component_index_[var] will be -1.
  std::vector<std::vector<int>> components_ ABSL_GUARDED_BY(graph_mutex_);
  std::vector<int> var_to_component_index_ ABSL_GUARDED_BY(graph_mutex_);

  // The set of active variables which is currently the list of non-constant
  // variables. It is stored both as a list and as a set (using a Boolean
  // vector).
  std::vector<bool> active_variables_set_ ABSL_GUARDED_BY(graph_mutex_);
  std::vector<int> active_variables_ ABSL_GUARDED_BY(graph_mutex_);

  // The list of non constant variables appearing in the objective.
  std::vector<int> active_objective_variables_ ABSL_GUARDED_BY(graph_mutex_);

  mutable absl::Mutex domain_mutex_;

  // Used to display periodic info to the log.
  absl::Time last_logging_time_;
};

// Base class for a CpModelProto neighborhood generator.
class NeighborhoodGenerator {
 public:
  NeighborhoodGenerator(const std::string& name,
                        NeighborhoodGeneratorHelper const* helper)
      : name_(name), helper_(*helper), difficulty_(0.5) {}
  virtual ~NeighborhoodGenerator() {}

  // Generates a "local" subproblem for the given seed.
  //
  // The difficulty will be in [0, 1] and is related to the asked neighborhood
  // size (and thus local problem difficulty). A difficulty of 0.0 means empty
  // neighborhood and a difficulty of 1.0 means the full problem. The algorithm
  // should try to generate a neighborhood according to this difficulty which
  // will be dynamically adjusted depending on whether or not we can solve the
  // subproblem in a given time limit.
  //
  // The given initial_solution should contain a feasible solution to the
  // initial CpModelProto given to this class. Any solution to the returned
  // CPModelProto should also be valid solution to the same initial model.
  //
  // This function should be thread-safe.
  virtual Neighborhood Generate(const CpSolverResponse& initial_solution,
                                double difficulty, absl::BitGenRef random) = 0;

  // Returns true if the neighborhood generator can generate a neighborhood.
  virtual bool ReadyToGenerate() const;

  // Uses UCB1 algorithm to compute the score (Multi armed bandit problem).
  // Details are at
  // https://lilianweng.github.io/lil-log/2018/01/23/the-multi-armed-bandit-problem-and-its-solutions.html.
  // 'total_num_calls' should be the sum of calls across all generators part of
  // the multi armed bandit problem.
  // If the generator is called less than 10 times then the method returns
  // infinity as score in order to get more data about the generator
  // performance.
  double GetUCBScore(int64_t total_num_calls) const;

  // Adds solve data about one "solved" neighborhood.
  struct SolveData {
    // The status of the sub-solve.
    CpSolverStatus status = CpSolverStatus::UNKNOWN;

    // The difficulty when this neighborhood was generated.
    double difficulty = 0.0;

    // The determinitic time limit given to the solver for this neighborhood.
    double deterministic_limit = 0.0;

    // The time it took to solve this neighborhood.
    double deterministic_time = 0.0;

    // Objective information. These only refer to the "internal" objective
    // without scaling or offset so we are exact and it is always in the
    // minimization direction.
    // - The initial best objective is the one of the best known solution at the
    //   time the neighborhood was generated.
    // - The base objective is the one of the base solution from which this
    //   neighborhood was generated.
    // - The new objective is the objective of the best solution found by
    //   solving the neighborhood.
    IntegerValue initial_best_objective = IntegerValue(0);
    IntegerValue base_objective = IntegerValue(0);
    IntegerValue new_objective = IntegerValue(0);

    // This is just used to construct a deterministic order for the updates.
    bool operator<(const SolveData& o) const {
      return std::tie(status, difficulty, deterministic_limit,
                      deterministic_time, initial_best_objective,
                      base_objective, new_objective) <
             std::tie(o.status, o.difficulty, o.deterministic_limit,
                      o.deterministic_time, o.initial_best_objective,
                      o.base_objective, o.new_objective);
    }
  };
  void AddSolveData(SolveData data) {
    absl::MutexLock mutex_lock(&generator_mutex_);
    solve_data_.push_back(data);
  }

  // Process all the recently added solve data and update this generator
  // score and difficulty.
  void Synchronize();

  // Returns a short description of the generator.
  std::string name() const { return name_; }

  // Number of times this generator was called.
  int64_t num_calls() const {
    absl::MutexLock mutex_lock(&generator_mutex_);
    return num_calls_;
  }

  // Number of time the neighborhood was fully solved (OPTIMAL/INFEASIBLE).
  int64_t num_fully_solved_calls() const {
    absl::MutexLock mutex_lock(&generator_mutex_);
    return num_fully_solved_calls_;
  }

  // The current difficulty of this generator
  double difficulty() const {
    absl::MutexLock mutex_lock(&generator_mutex_);
    return difficulty_.value();
  }

  // The current time limit that the sub-solve should use on this generator.
  double deterministic_limit() const {
    absl::MutexLock mutex_lock(&generator_mutex_);
    return deterministic_limit_;
  }

  // The sum of the deterministic time spent in this generator.
  double deterministic_time() const {
    absl::MutexLock mutex_lock(&generator_mutex_);
    return deterministic_time_;
  }

 protected:
  const std::string name_;
  const NeighborhoodGeneratorHelper& helper_;
  mutable absl::Mutex generator_mutex_;

 private:
  std::vector<SolveData> solve_data_;

  // Current parameters to be used when generating/solving a neighborhood with
  // this generator. Only updated on Synchronize().
  AdaptiveParameterValue difficulty_;
  double deterministic_limit_ = 0.1;

  // Current statistics of the last solved neighborhood.
  // Only updated on Synchronize().
  int64_t num_calls_ = 0;
  int64_t num_fully_solved_calls_ = 0;
  int64_t num_consecutive_non_improving_calls_ = 0;
  double deterministic_time_ = 0.0;
  double current_average_ = 0.0;
};

// Pick a random subset of variables.
//
// TODO(user): In the presence of connected components, this should just work
// on one of them.
class RelaxRandomVariablesGenerator : public NeighborhoodGenerator {
 public:
  explicit RelaxRandomVariablesGenerator(
      NeighborhoodGeneratorHelper const* helper, const std::string& name)
      : NeighborhoodGenerator(name, helper) {}
  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, absl::BitGenRef random) final;
};

// Pick a random subset of constraints and relax all the variables of these
// constraints. Note that to satisfy the difficulty, we might not relax all the
// variable of the "last" constraint.
//
// TODO(user): In the presence of connected components, this should just work
// on one of them.
class RelaxRandomConstraintsGenerator : public NeighborhoodGenerator {
 public:
  explicit RelaxRandomConstraintsGenerator(
      NeighborhoodGeneratorHelper const* helper, const std::string& name)
      : NeighborhoodGenerator(name, helper) {}
  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, absl::BitGenRef random) final;
};

// Pick a random subset of variables that are constructed by a BFS in the
// variable <-> constraint graph. That is, pick a random variable, then all the
// variable connected by some constraint to the first one, and so on. The
// variable of the last "level" are selected randomly.
//
// Note that in the presence of connected component, this works correctly
// already.
class VariableGraphNeighborhoodGenerator : public NeighborhoodGenerator {
 public:
  explicit VariableGraphNeighborhoodGenerator(
      NeighborhoodGeneratorHelper const* helper, const std::string& name)
      : NeighborhoodGenerator(name, helper) {}
  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, absl::BitGenRef random) final;
};

// Pick a random subset of constraint and relax all of their variables. We are a
// bit smarter than this because after the first constraint is selected, we only
// select constraints that share at least one variable with the already selected
// constraints. The variable from the "last" constraint are selected randomly.
class ConstraintGraphNeighborhoodGenerator : public NeighborhoodGenerator {
 public:
  explicit ConstraintGraphNeighborhoodGenerator(
      NeighborhoodGeneratorHelper const* helper, const std::string& name)
      : NeighborhoodGenerator(name, helper) {}
  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, absl::BitGenRef random) final;
};

// Pick a random subset of objective terms.
class RelaxObjectiveVariablesGenerator : public NeighborhoodGenerator {
 public:
  explicit RelaxObjectiveVariablesGenerator(
      NeighborhoodGeneratorHelper const* helper, const std::string& name)
      : NeighborhoodGenerator(name, helper) {}
  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, absl::BitGenRef random) final;
};

// Helper method for the scheduling neighborhood generators. Returns a
// neighborhood defined from the given set of intervals to relax. For each
// scheduling constraint, it adds strict relation order between the non-relaxed
// intervals.
Neighborhood GenerateSchedulingNeighborhoodFromRelaxedIntervals(
    const absl::Span<const int> intervals_to_relax,
    const CpSolverResponse& initial_solution, absl::BitGenRef random,
    const NeighborhoodGeneratorHelper& helper);

// Helper method for the scheduling neighborhood generators. Returns a
// full neighborhood enriched with the set or precedences passed to the generate
// method.
Neighborhood GenerateSchedulingNeighborhoodFromIntervalPrecedences(
    const absl::Span<const std::pair<int, int>> precedences,
    const CpSolverResponse& initial_solution,
    const NeighborhoodGeneratorHelper& helper);

// Only make sense for scheduling problem. This select a random set of interval
// of the problem according to the difficulty. Then, for each scheduling
// constraints, it adds strict relation order between the non-relaxed intervals.
class RandomIntervalSchedulingNeighborhoodGenerator
    : public NeighborhoodGenerator {
 public:
  explicit RandomIntervalSchedulingNeighborhoodGenerator(
      NeighborhoodGeneratorHelper const* helper, const std::string& name)
      : NeighborhoodGenerator(name, helper) {}

  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, absl::BitGenRef random) final;
};

// Only make sense for scheduling problem. This select a random set of
// precedences between intervals of the problem according to the difficulty.
// These precedences are extracted from the scheduling constraints and their
// configuration in the current solution. Then it adds the kept precedences to
// the model.
class RandomPrecedenceSchedulingNeighborhoodGenerator
    : public NeighborhoodGenerator {
 public:
  explicit RandomPrecedenceSchedulingNeighborhoodGenerator(
      NeighborhoodGeneratorHelper const* helper, const std::string& name)
      : NeighborhoodGenerator(name, helper) {}

  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, absl::BitGenRef random) final;
};

// Similar to SchedulingNeighborhoodGenerator except the set of intervals that
// are relaxed are from a specific random time interval.
class SchedulingTimeWindowNeighborhoodGenerator : public NeighborhoodGenerator {
 public:
  explicit SchedulingTimeWindowNeighborhoodGenerator(
      NeighborhoodGeneratorHelper const* helper, const std::string& name)
      : NeighborhoodGenerator(name, helper) {}

  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, absl::BitGenRef random) final;
};

// Similar to SchedulingTimeWindowNeighborhoodGenerator except that it relaxes
// one independent time window per resource (1 for each dimension in the
// no_overlap_2d case).
class SchedulingResourceWindowsNeighborhoodGenerator
    : public NeighborhoodGenerator {
 public:
  explicit SchedulingResourceWindowsNeighborhoodGenerator(
      NeighborhoodGeneratorHelper const* helper,
      const std::vector<std::vector<int>>& intervals_in_constraints,
      const std::string& name)
      : NeighborhoodGenerator(name, helper),
        intervals_in_constraints_(intervals_in_constraints) {}

  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, absl::BitGenRef random) final;

 private:
  const std::vector<std::vector<int>> intervals_in_constraints_;
  absl::flat_hash_set<int> intervals_to_relax_;
};

// This routing based LNS generator will relax random arcs in all the paths of
// the circuit or routes constraints.
class RoutingRandomNeighborhoodGenerator : public NeighborhoodGenerator {
 public:
  RoutingRandomNeighborhoodGenerator(NeighborhoodGeneratorHelper const* helper,
                                     const std::string& name)
      : NeighborhoodGenerator(name, helper) {}

  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, absl::BitGenRef random) final;
};

// This routing based LNS generator will relax small sequences of arcs randomly
// chosen in all the paths of the circuit or routes constraints.
class RoutingPathNeighborhoodGenerator : public NeighborhoodGenerator {
 public:
  RoutingPathNeighborhoodGenerator(NeighborhoodGeneratorHelper const* helper,
                                   const std::string& name)
      : NeighborhoodGenerator(name, helper) {}

  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, absl::BitGenRef random) final;
};

// This routing based LNS generator aims are relaxing one full path, and make
// some room on the other paths to absorb the nodes of the relaxed path.
//
// In order to do so, it will relax the first and the last arc of each path in
// the circuit or routes constraints. Then it will relax all arc literals in one
// random path. Then it will relax random arcs in the remaining paths until it
// reaches the given difficulty.
class RoutingFullPathNeighborhoodGenerator : public NeighborhoodGenerator {
 public:
  RoutingFullPathNeighborhoodGenerator(
      NeighborhoodGeneratorHelper const* helper, const std::string& name)
      : NeighborhoodGenerator(name, helper) {}

  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, absl::BitGenRef random) final;
};

// Generates a neighborhood by fixing the variables to solutions reported in
// various repositories. This is inspired from RINS published in "Exploring
// relaxation induced neighborhoods to improve MIP solutions" 2004 by E. Danna
// et.
//
// If incomplete_solutions is provided, this generates a neighborhood by fixing
// the variable values to a solution in the SharedIncompleteSolutionManager and
// ignores the other repositories.
//
// Otherwise, if response_manager is not provided, this generates a neighborhood
// using only the linear/general relaxation values. The domain of the variables
// are reduced to the integer values around their lp solution/relaxation
// solution values. This was published in "RENS – The Relaxation Enforced
// Neighborhood" 2009 by Timo Berthold.
class RelaxationInducedNeighborhoodGenerator : public NeighborhoodGenerator {
 public:
  explicit RelaxationInducedNeighborhoodGenerator(
      NeighborhoodGeneratorHelper const* helper,
      const SharedResponseManager* response_manager,
      const SharedRelaxationSolutionRepository* relaxation_solutions,
      const SharedLPSolutionRepository* lp_solutions,
      SharedIncompleteSolutionManager* incomplete_solutions,
      const std::string& name)
      : NeighborhoodGenerator(name, helper),
        response_manager_(response_manager),
        relaxation_solutions_(relaxation_solutions),
        lp_solutions_(lp_solutions),
        incomplete_solutions_(incomplete_solutions) {
    CHECK(lp_solutions_ != nullptr || relaxation_solutions_ != nullptr ||
          incomplete_solutions != nullptr);
  }

  // Both initial solution and difficulty values are ignored.
  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, absl::BitGenRef random) final;

  // Returns true if the required solutions are available.
  bool ReadyToGenerate() const override;

 private:
  const SharedResponseManager* response_manager_;
  const SharedRelaxationSolutionRepository* relaxation_solutions_;
  const SharedLPSolutionRepository* lp_solutions_;
  SharedIncompleteSolutionManager* incomplete_solutions_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_LNS_H_
