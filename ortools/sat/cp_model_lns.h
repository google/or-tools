// Copyright 2010-2018 Google LLC
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

#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "ortools/base/integral_types.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/model.h"
#include "ortools/sat/subsolver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/adaptative_parameter_value.h"
#include "ortools/util/random_engine.h"

namespace operations_research {
namespace sat {

// Neighborhood returned by Neighborhood generators.
struct Neighborhood {
  // True if neighborhood generator was able to generate a neighborhood.
  bool is_generated = false;

  // True if the CpModelProto below is not the same as the base model.
  // This is not expected to happen often but allows to handle this case
  // properly.
  bool is_reduced = false;

  // Relaxed model. Any feasible solution to this "local" model should be a
  // feasible solution to the base model too.
  CpModelProto cp_model;

  // Neighborhood Id. Used to identify the neighborhood by a generator.
  // Currently only used by WeightedRandomRelaxationNeighborhoodGenerator.
  // TODO(user): Make sure that the id is unique for each generated
  // neighborhood for each generator.
  int64 id = 0;

  // Used for identifying the source of the neighborhood if it is generated
  // using solution repositories.
  std::string source_info = "";
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
                              SharedTimeLimit* shared_time_limit = nullptr,
                              SharedBoundsManager* shared_bounds = nullptr);

  // SubSolver interface.
  bool TaskIsAvailable() override { return false; }
  std::function<void()> GenerateTask(int64 task_id) override { return {}; }
  void Synchronize() override;

  // Returns the LNS fragment where the given variables are fixed to the value
  // they take in the given solution.
  Neighborhood FixGivenVariables(
      const CpSolverResponse& initial_solution,
      const std::vector<int>& variables_to_fix) const;

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

  // Return a neighborhood that correspond to the full problem.
  Neighborhood FullNeighborhood() const;

  // Indicates if the variable can be frozen. It happens if the variable is non
  // constant, and if it is a decision variable, or if
  // focus_on_decision_variables is false.
  bool IsActive(int var) const;

  // Returns the list of "active" variables.
  const std::vector<int>& ActiveVariables() const { return active_variables_; }

  // Constraints <-> Variables graph.
  // Note that only non-constant variable are listed here.
  const std::vector<std::vector<int>>& ConstraintToVar() const {
    return constraint_to_var_;
  }
  const std::vector<std::vector<int>>& VarToConstraint() const {
    return var_to_constraint_;
  }

  // Returns all the constraints indices of a given type.
  const absl::Span<const int> TypeToConstraints(
      ConstraintProto::ConstraintCase type) const {
    if (type >= type_to_constraints_.size()) return {};
    return absl::MakeSpan(type_to_constraints_[type]);
  }

  // The initial problem.
  // Note that the domain of the variables are not updated here.
  const CpModelProto& ModelProto() const { return model_proto_; }
  const SatParameters& Parameters() const { return parameters_; }

  // This mutex must be acquired before calling any of the function that access
  // data that can be updated by Synchronize().
  //
  // TODO(user): Refactor the class to be thread-safe instead, it should be
  // safer and more easily maintenable. Some complication with accessing the
  // variable<->constraint graph efficiently though.
  absl::Mutex* MutableMutex() const { return &mutex_; }

  const SharedResponseManager& shared_response() const {
    return *shared_response_;
  }

 private:
  // Recompute most of the class member. This needs to be called when the
  // domains of the variables are updated.
  void RecomputeHelperData();

  // Indicates if a variable is fixed in the model.
  bool IsConstant(int var) const;

  const SatParameters& parameters_;
  const CpModelProto& model_proto_;
  int shared_bounds_id_;
  SharedTimeLimit* shared_time_limit_;
  SharedBoundsManager* shared_bounds_;
  SharedResponseManager* shared_response_;
  SharedRelaxationSolutionRepository* shared_relaxation_solutions_;

  // This proto will only contain the field variables() with an updated version
  // of the domains compared to model_proto_.variables(). We do it like this to
  // reduce the memory footprint of the helper when the model is large.
  //
  // TODO(user): Use custom domain repository rather than a proto?
  CpModelProto model_proto_with_only_variables_;

  mutable absl::Mutex mutex_;

  // Constraints by types.
  std::vector<std::vector<int>> type_to_constraints_;

  // Variable-Constraint graph.
  std::vector<std::vector<int>> constraint_to_var_;
  std::vector<std::vector<int>> var_to_constraint_;

  // The set of active variables, that is the list of non constant variables if
  // parameters_.focus_on_decision_variables() is false, or the list of non
  // constant decision variables otherwise. It is stored both as a list and as a
  // set (using a Boolean vector).
  std::vector<bool> active_variables_set_;
  std::vector<int> active_variables_;
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
                                double difficulty, random_engine_t* random) = 0;

  // Returns true if the neighborhood generator can generate a neighborhood.
  virtual bool ReadyToGenerate() const;

  // Returns true if the neighborhood generator generates relaxation of the
  // given problem.
  virtual bool IsRelaxationGenerator() const { return false; }

  // Uses UCB1 algorithm to compute the score (Multi armed bandit problem).
  // Details are at
  // https://lilianweng.github.io/lil-log/2018/01/23/the-multi-armed-bandit-problem-and-its-solutions.html.
  // 'total_num_calls' should be the sum of calls across all generators part of
  // the multi armed bandit problem.
  // If the generator is called less than 10 times then the method returns
  // infinity as score in order to get more data about the generator
  // performance.
  double GetUCBScore(int64 total_num_calls) const;

  // Adds solve data about one "solved" neighborhood.
  struct SolveData {
    // Neighborhood Id. Used to identify the neighborhood by a generator.
    // Currently only used by WeightedRandomRelaxationNeighborhoodGenerator.
    int64 neighborhood_id = 0;

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

    // Bounds data is only used by relaxation neighborhoods.
    IntegerValue initial_best_objective_bound = IntegerValue(0);
    IntegerValue new_objective_bound = IntegerValue(0);

    // This is just used to construct a deterministic order for the updates.
    bool operator<(const SolveData& o) const {
      return std::tie(status, difficulty, deterministic_limit,
                      deterministic_time, initial_best_objective,
                      base_objective, new_objective,
                      initial_best_objective_bound, new_objective_bound,
                      neighborhood_id) <
             std::tie(o.status, o.difficulty, o.deterministic_limit,
                      o.deterministic_time, o.initial_best_objective,
                      o.base_objective, o.new_objective,
                      o.initial_best_objective_bound, o.new_objective_bound,
                      o.neighborhood_id);
    }
  };
  void AddSolveData(SolveData data) {
    absl::MutexLock mutex_lock(&mutex_);
    solve_data_.push_back(data);
  }

  // Process all the recently added solve data and update this generator
  // score and difficulty.
  void Synchronize();

  // Returns a short description of the generator.
  std::string name() const { return name_; }

  // Number of times this generator was called.
  int64 num_calls() const {
    absl::MutexLock mutex_lock(&mutex_);
    return num_calls_;
  }

  // Number of time the neighborhood was fully solved (OPTIMAL/INFEASIBLE).
  int64 num_fully_solved_calls() const {
    absl::MutexLock mutex_lock(&mutex_);
    return num_fully_solved_calls_;
  }

  // The current difficulty of this generator
  double difficulty() const {
    absl::MutexLock mutex_lock(&mutex_);
    return difficulty_.value();
  }

  // The current time limit that the sub-solve should use on this generator.
  double deterministic_limit() const {
    absl::MutexLock mutex_lock(&mutex_);
    return deterministic_limit_;
  }

  // The sum of the deterministic time spent in this generator.
  double deterministic_time() const {
    absl::MutexLock mutex_lock(&mutex_);
    return deterministic_time_;
  }

 protected:
  // Triggered with each call to Synchronize() for each recently added
  // SolveData. This is meant to be used for processing feedbacks by specific
  // neighborhood generators to adjust the neighborhood generation process.
  virtual void AdditionalProcessingOnSynchronize(const SolveData& solve_data) {}

  const std::string name_;
  const NeighborhoodGeneratorHelper& helper_;
  mutable absl::Mutex mutex_;

 private:
  std::vector<SolveData> solve_data_;

  // Current parameters to be used when generating/solving a neighborhood with
  // this generator. Only updated on Synchronize().
  AdaptiveParameterValue difficulty_;
  double deterministic_limit_ = 0.1;

  // Current statistics of the last solved neighborhood.
  // Only updated on Synchronize().
  int64 num_calls_ = 0;
  int64 num_fully_solved_calls_ = 0;
  int64 num_consecutive_non_improving_calls_ = 0;
  double deterministic_time_ = 0.0;
  double current_average_ = 0.0;
};

// Pick a random subset of variables.
class SimpleNeighborhoodGenerator : public NeighborhoodGenerator {
 public:
  explicit SimpleNeighborhoodGenerator(
      NeighborhoodGeneratorHelper const* helper, const std::string& name)
      : NeighborhoodGenerator(name, helper) {}
  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, random_engine_t* random) final;
};

// Pick a random subset of variables that are constructed by a BFS in the
// variable <-> constraint graph. That is, pick a random variable, then all the
// variable connected by some constraint to the first one, and so on. The
// variable of the last "level" are selected randomly.
class VariableGraphNeighborhoodGenerator : public NeighborhoodGenerator {
 public:
  explicit VariableGraphNeighborhoodGenerator(
      NeighborhoodGeneratorHelper const* helper, const std::string& name)
      : NeighborhoodGenerator(name, helper) {}
  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, random_engine_t* random) final;
};

// Pick a random subset of constraint and relax all of their variables. We are a
// bit smarter than this because after the first contraint is selected, we only
// select constraints that share at least one variable with the already selected
// constraints. The variable from the "last" constraint are selected randomly.
class ConstraintGraphNeighborhoodGenerator : public NeighborhoodGenerator {
 public:
  explicit ConstraintGraphNeighborhoodGenerator(
      NeighborhoodGeneratorHelper const* helper, const std::string& name)
      : NeighborhoodGenerator(name, helper) {}
  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, random_engine_t* random) final;
};

// Helper method for the scheduling neighborhood generators. Returns the model
// as neighborhood for the given set of intervals to relax. For each no_overlap
// constraints, it adds strict relation order between the non-relaxed intervals.
Neighborhood GenerateSchedulingNeighborhoodForRelaxation(
    const absl::Span<const int> intervals_to_relax,
    const CpSolverResponse& initial_solution,
    const NeighborhoodGeneratorHelper& helper);

// Only make sense for scheduling problem. This select a random set of interval
// of the problem according to the difficulty. Then, for each no_overlap
// constraints, it adds strict relation order between the non-relaxed intervals.
//
// TODO(user): Also deal with cumulative constraint.
class SchedulingNeighborhoodGenerator : public NeighborhoodGenerator {
 public:
  explicit SchedulingNeighborhoodGenerator(
      NeighborhoodGeneratorHelper const* helper, const std::string& name)
      : NeighborhoodGenerator(name, helper) {}

  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, random_engine_t* random) final;
};

// Similar to SchedulingNeighborhoodGenerator except the set of intervals that
// are relaxed are from a specific random time interval.
class SchedulingTimeWindowNeighborhoodGenerator : public NeighborhoodGenerator {
 public:
  explicit SchedulingTimeWindowNeighborhoodGenerator(
      NeighborhoodGeneratorHelper const* helper, const std::string& name)
      : NeighborhoodGenerator(name, helper) {}

  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, random_engine_t* random) final;
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
                        double difficulty, random_engine_t* random) final;

  // Returns true if the required solutions are available.
  bool ReadyToGenerate() const override;

 private:
  const SharedResponseManager* response_manager_;
  const SharedRelaxationSolutionRepository* relaxation_solutions_;
  const SharedLPSolutionRepository* lp_solutions_;
  SharedIncompleteSolutionManager* incomplete_solutions_;
};

// Generates a relaxation of the original model by removing a consecutive span
// of constraints starting at a random index. The number of constraints removed
// is in sync with the difficulty passed to the generator.
class ConsecutiveConstraintsRelaxationNeighborhoodGenerator
    : public NeighborhoodGenerator {
 public:
  explicit ConsecutiveConstraintsRelaxationNeighborhoodGenerator(
      NeighborhoodGeneratorHelper const* helper, const std::string& name)
      : NeighborhoodGenerator(name, helper) {}
  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, random_engine_t* random) final;

  bool IsRelaxationGenerator() const override { return true; }
  bool ReadyToGenerate() const override { return true; }
};

// Generates a relaxation of the original model by removing some constraints
// randomly with a given weight for each constraint that controls the
// probability of constraint getting removed. The number of constraints removed
// is in sync with the difficulty passed to the generator. Higher weighted
// constraints are more likely to get removed.
class WeightedRandomRelaxationNeighborhoodGenerator
    : public NeighborhoodGenerator {
 public:
  WeightedRandomRelaxationNeighborhoodGenerator(
      NeighborhoodGeneratorHelper const* helper, const std::string& name);

  // Generates the neighborhood as described above. Also stores the removed
  // constraints indices for adjusting the weights.
  Neighborhood Generate(const CpSolverResponse& initial_solution,
                        double difficulty, random_engine_t* random) final;

  bool IsRelaxationGenerator() const override { return true; }
  bool ReadyToGenerate() const override { return true; }

 private:
  // Adjusts the weights of the constraints removed to get the neighborhood
  // based on the solve_data.
  void AdditionalProcessingOnSynchronize(const SolveData& solve_data) override
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Higher weighted constraints are more likely to get removed.
  std::vector<double> constraint_weights_;
  int num_removable_constraints_ = 0;

  // Indices of the removed constraints per generated neighborhood.
  absl::flat_hash_map<int64, std::vector<int>> removed_constraints_
      ABSL_GUARDED_BY(mutex_);

  // TODO(user): Move this to parent class if other generators start using
  // feedbacks.
  int64 next_available_id_ ABSL_GUARDED_BY(mutex_) = 0;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_LNS_H_
