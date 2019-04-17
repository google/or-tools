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

#include "absl/types/span.h"
#include "ortools/base/integral_types.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

// Neighborhood returned by Neighborhood generators.
struct Neighborhood {
  // True if the CpModelProto below is not the same as the base model.
  // This is not expected to happen often but allows to handle this case
  // properly.
  bool is_reduced;

  // Relaxed model. Any feasible solution to this "local" model should be a
  // feasible solution to the base model too.
  CpModelProto cp_model;
};

// Contains pre-computed information about a given CpModelProto that is meant
// to be used to generate LNS neighborhood. This class can be shared between
// more than one generator in order to reduce memory usage.
//
// Thread-safe.
class NeighborhoodGeneratorHelper {
 public:
  NeighborhoodGeneratorHelper(CpModelProto const* model_proto,
                              bool focus_on_decision_variables);

  // Updates private variables using the current 'model_proto_'.
  void UpdateHelperData(bool focus_on_decision_variables);

  // Returns the LNS fragment where the given variables are fixed to the value
  // they take in the given solution.
  Neighborhood FixGivenVariables(
      const CpSolverResponse& initial_solution,
      const std::vector<int>& variables_to_fix) const;

  // Returns the LNS fragment which will relax all inactive variables and all
  // variables in relaxed_variables.
  Neighborhood RelaxGivenVariables(
      const CpSolverResponse& initial_solution,
      const std::vector<int>& relaxed_variables) const;

  // Returns a trivial model by fixing all active variables to the initial
  // solution values.
  Neighborhood FixAllVariables(const CpSolverResponse& initial_solution) const;

  // Indicates if the variable can be frozen. It happens if the variable is non
  // constant, and if it is a decision variable, or if
  // focus_on_decision_variables is false.
  bool IsActive(int var) const;

  // Returns the list of "active" variables.
  const std::vector<int>& ActiveVariables() const { return active_variables_; }

  // Constraints <-> Variables graph.
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
  const CpModelProto& ModelProto() const { return model_proto_; }

 private:
  // Indicates if a variable is fixed in the model.
  bool IsConstant(int var) const;

  const CpModelProto& model_proto_;

  // Constraints by types.
  std::vector<std::vector<int>> type_to_constraints_;

  // Variable-Constraint graph.
  std::vector<std::vector<int>> constraint_to_var_;
  std::vector<std::vector<int>> var_to_constraint_;

  // The set of active variables, that is the list of non constant variables
  // if focus_on_decision_variables_ is false, or the list of non constant
  // decision variables otherwise.
  // It is stored both as a list and as a set (using a Boolean vector).
  std::vector<bool> active_variables_set_;
  std::vector<int> active_variables_;
};

// Base class for a CpModelProto neighborhood generator.
class NeighborhoodGenerator {
 public:
  NeighborhoodGenerator(const std::string& name,
                        NeighborhoodGeneratorHelper const* helper)
      : helper_(*helper), name_(name) {}
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
  // The given initial_solution should contains a feasible solution to the
  // initial CpModelProto given to this class. Any solution to the returned
  // CPModelProto should also be valid solution to the same initial model.
  //
  // This function should be thread-safe.
  virtual Neighborhood Generate(const CpSolverResponse& initial_solution,
                                int64 seed, double difficulty) const = 0;

  // Returns a short description of the generator.
  std::string name() const { return name_; }

  // Uses UCB1 algorithm to compute the score (Multi armed bandit problem).
  // Details are at
  // https://lilianweng.github.io/lil-log/2018/01/23/the-multi-armed-bandit-problem-and-its-solutions.html.
  // 'total_num_calls' should be the sum of calls across all generators part of
  // the multi armed bandit problem.
  // If the generator is called less than 10 times then the method returns
  // inifinity as score in order to get more data about the generator
  // performance.
  double GetUCBScore(int64 total_num_calls) const;

  // Updates the records using the current improvement in objective for the
  // generator.
  void AddSolveData(double objective_diff, double deterministic_time);

  // Number of times this generator is called.
  int64 num_calls() const { return num_calls_; }

 protected:
  const NeighborhoodGeneratorHelper& helper_;
  const std::string name_;
  int64 num_calls_ = 0;
  double current_average_ = 0.0;
};

// Pick a random subset of variables.
class SimpleNeighborhoodGenerator : public NeighborhoodGenerator {
 public:
  explicit SimpleNeighborhoodGenerator(
      NeighborhoodGeneratorHelper const* helper, const std::string& name)
      : NeighborhoodGenerator(name, helper) {}
  Neighborhood Generate(const CpSolverResponse& initial_solution, int64 seed,
                        double difficulty) const final;
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
  Neighborhood Generate(const CpSolverResponse& initial_solution, int64 seed,
                        double difficulty) const final;
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
  Neighborhood Generate(const CpSolverResponse& initial_solution, int64 seed,
                        double difficulty) const final;
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

  Neighborhood Generate(const CpSolverResponse& initial_solution, int64 seed,
                        double difficulty) const final;
};

// Similar to SchedulingNeighborhoodGenerator except the set of intervals that
// are relaxed are from a specific random time interval.
class SchedulingTimeWindowNeighborhoodGenerator : public NeighborhoodGenerator {
 public:
  explicit SchedulingTimeWindowNeighborhoodGenerator(
      NeighborhoodGeneratorHelper const* helper, const std::string& name)
      : NeighborhoodGenerator(name, helper) {}

  Neighborhood Generate(const CpSolverResponse& initial_solution, int64 seed,
                        double difficulty) const final;
};

// Generates a neighborhood by fixing the variables who have same solution value
// as their linear relaxation. This was published in "Exploring relaxation
// induced neighborhoods to improve MIP solutions" 2004 by E. Danna et.
// TODO(user): This simulates RINS only at root node. Try to exploit it on
// other nodes as well.
class RelaxationInducedNeighborhoodGenerator : public NeighborhoodGenerator {
 public:
  explicit RelaxationInducedNeighborhoodGenerator(
      NeighborhoodGeneratorHelper const* helper, Model* model,
      const std::string& name)
      : NeighborhoodGenerator(name, helper), model_(model) {}

  Neighborhood Generate(const CpSolverResponse& initial_solution, int64 seed,
                        double difficulty) const final;

  const Model* model_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_LNS_H_
