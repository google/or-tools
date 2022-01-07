// Copyright 2010-2021 Google LLC
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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_LP_SCHEDULING_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_LP_SCHEDULING_H_

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/time/time.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {

// Classes to solve dimension cumul placement (aka scheduling) problems using
// linear programming.

// Utility class used in the core optimizer to tighten the cumul bounds as much
// as possible based on the model precedences.
class CumulBoundsPropagator {
 public:
  explicit CumulBoundsPropagator(const RoutingDimension* dimension);

  // Tightens the cumul bounds starting from the current cumul var min/max,
  // and propagating the precedences resulting from the next_accessor, and the
  // dimension's precedence rules.
  // Returns false iff the precedences are infeasible with the given routes.
  // Otherwise, the user can call CumulMin() and CumulMax() to retrieve the new
  // bounds of an index.
  bool PropagateCumulBounds(
      const std::function<int64_t(int64_t)>& next_accessor,
      int64_t cumul_offset);

  int64_t CumulMin(int index) const {
    return propagated_bounds_[PositiveNode(index)];
  }

  int64_t CumulMax(int index) const {
    const int64_t negated_upper_bound = propagated_bounds_[NegativeNode(index)];
    return negated_upper_bound == std::numeric_limits<int64_t>::min()
               ? std::numeric_limits<int64_t>::max()
               : -negated_upper_bound;
  }

  const RoutingDimension& dimension() const { return dimension_; }

 private:
  // An arc "tail --offset--> head" represents the relation
  // tail + offset <= head.
  // As arcs are stored by tail, we don't store it in the struct.
  struct ArcInfo {
    int head;
    int64_t offset;
  };
  static const int kNoParent;
  static const int kParentToBePropagated;

  // Return the node corresponding to the lower bound of the cumul of index and
  // -index respectively.
  int PositiveNode(int index) const { return 2 * index; }
  int NegativeNode(int index) const { return 2 * index + 1; }

  void AddNodeToQueue(int node) {
    if (!node_in_queue_[node]) {
      bf_queue_.push_back(node);
      node_in_queue_[node] = true;
    }
  }

  // Adds the relation first_index + offset <= second_index, by adding arcs
  // first_index --offset--> second_index and
  // -second_index --offset--> -first_index.
  void AddArcs(int first_index, int second_index, int64_t offset);

  bool InitializeArcsAndBounds(
      const std::function<int64_t(int64_t)>& next_accessor,
      int64_t cumul_offset);

  bool UpdateCurrentLowerBoundOfNode(int node, int64_t new_lb, int64_t offset);

  bool DisassembleSubtree(int source, int target);

  bool CleanupAndReturnFalse() {
    // We clean-up node_in_queue_ for future calls, and return false.
    for (int node_to_cleanup : bf_queue_) {
      node_in_queue_[node_to_cleanup] = false;
    }
    bf_queue_.clear();
    return false;
  }

  const RoutingDimension& dimension_;
  const int64_t num_nodes_;

  // TODO(user): Investigate if all arcs for a given tail can be created
  // at the same time, in which case outgoing_arcs_ could point to an absl::Span
  // for each tail index.
  std::vector<std::vector<ArcInfo>> outgoing_arcs_;

  std::deque<int> bf_queue_;
  std::vector<bool> node_in_queue_;
  std::vector<int> tree_parent_node_of_;
  // After calling PropagateCumulBounds(), for each node index n,
  // propagated_bounds_[2*n] and -propagated_bounds_[2*n+1] respectively contain
  // the propagated lower and upper bounds of n's cumul variable.
  std::vector<int64_t> propagated_bounds_;

  // Vector used in DisassembleSubtree() to avoid memory reallocation.
  std::vector<int> tmp_dfs_stack_;

  // Used to store the pickup/delivery pairs encountered on the routes.
  std::vector<std::pair<int64_t, int64_t>>
      visited_pickup_delivery_indices_for_pair_;
};

enum class DimensionSchedulingStatus {
  // An optimal solution was found respecting all constraints.
  OPTIMAL,
  // An optimal solution was found, however constraints which were relaxed were
  // violated.
  RELAXED_OPTIMAL_ONLY,
  // A solution could not be found.
  INFEASIBLE
};

class RoutingLinearSolverWrapper {
 public:
  virtual ~RoutingLinearSolverWrapper() {}
  virtual void Clear() = 0;
  virtual int CreateNewPositiveVariable() = 0;
  virtual bool SetVariableBounds(int index, int64_t lower_bound,
                                 int64_t upper_bound) = 0;
  virtual void SetVariableDisjointBounds(int index,
                                         const std::vector<int64_t>& starts,
                                         const std::vector<int64_t>& ends) = 0;
  virtual int64_t GetVariableLowerBound(int index) const = 0;
  virtual int64_t GetVariableUpperBound(int index) const = 0;
  virtual void SetObjectiveCoefficient(int index, double coefficient) = 0;
  virtual double GetObjectiveCoefficient(int index) const = 0;
  virtual void ClearObjective() = 0;
  virtual int NumVariables() const = 0;
  virtual int CreateNewConstraint(int64_t lower_bound, int64_t upper_bound) = 0;
  virtual void SetCoefficient(int ct, int index, double coefficient) = 0;
  virtual bool IsCPSATSolver() = 0;
  virtual void AddObjectiveConstraint() = 0;
  virtual void AddMaximumConstraint(int max_var, std::vector<int> vars) = 0;
  virtual void AddProductConstraint(int product_var, std::vector<int> vars) = 0;
  virtual void SetEnforcementLiteral(int ct, int condition) = 0;
  virtual DimensionSchedulingStatus Solve(absl::Duration duration_limit) = 0;
  virtual int64_t GetObjectiveValue() const = 0;
  virtual double GetValue(int index) const = 0;
  virtual bool SolutionIsInteger() const = 0;

  // Adds a variable with bounds [lower_bound, upper_bound].
  int AddVariable(int64_t lower_bound, int64_t upper_bound) {
    CHECK_LE(lower_bound, upper_bound);
    const int variable = CreateNewPositiveVariable();
    SetVariableBounds(variable, lower_bound, upper_bound);
    return variable;
  }
  // Adds a linear constraint, enforcing
  // lower_bound <= sum variable * coeff <= upper_bound,
  // and returns the identifier of that constraint.
  int AddLinearConstraint(
      int64_t lower_bound, int64_t upper_bound,
      const std::vector<std::pair<int, double>>& variable_coeffs) {
    CHECK_LE(lower_bound, upper_bound);
    const int ct = CreateNewConstraint(lower_bound, upper_bound);
    for (const auto& variable_coeff : variable_coeffs) {
      SetCoefficient(ct, variable_coeff.first, variable_coeff.second);
    }
    return ct;
  }
  // Adds a linear constraint and a 0/1 variable that is true iff
  // lower_bound <= sum variable * coeff <= upper_bound,
  // and returns the identifier of that variable.
  int AddReifiedLinearConstraint(
      int64_t lower_bound, int64_t upper_bound,
      const std::vector<std::pair<int, double>>& weighted_variables) {
    const int reification_ct = AddLinearConstraint(1, 1, {});
    if (std::numeric_limits<int64_t>::min() < lower_bound) {
      const int under_lower_bound = AddVariable(0, 1);
      SetCoefficient(reification_ct, under_lower_bound, 1);
      const int under_lower_bound_ct =
          AddLinearConstraint(std::numeric_limits<int64_t>::min(),
                              lower_bound - 1, weighted_variables);
      SetEnforcementLiteral(under_lower_bound_ct, under_lower_bound);
    }
    if (upper_bound < std::numeric_limits<int64_t>::max()) {
      const int above_upper_bound = AddVariable(0, 1);
      SetCoefficient(reification_ct, above_upper_bound, 1);
      const int above_upper_bound_ct = AddLinearConstraint(
          upper_bound + 1, std::numeric_limits<int64_t>::max(),
          weighted_variables);
      SetEnforcementLiteral(above_upper_bound_ct, above_upper_bound);
    }
    const int within_bounds = AddVariable(0, 1);
    SetCoefficient(reification_ct, within_bounds, 1);
    const int within_bounds_ct =
        AddLinearConstraint(lower_bound, upper_bound, weighted_variables);
    SetEnforcementLiteral(within_bounds_ct, within_bounds);
    return within_bounds;
  }
};

class RoutingGlopWrapper : public RoutingLinearSolverWrapper {
 public:
  RoutingGlopWrapper(bool is_relaxation, const glop::GlopParameters& parameters)
      : is_relaxation_(is_relaxation) {
    lp_solver_.SetParameters(parameters);
    linear_program_.SetMaximizationProblem(false);
  }
  void Clear() override {
    linear_program_.Clear();
    linear_program_.SetMaximizationProblem(false);
    allowed_intervals_.clear();
  }
  int CreateNewPositiveVariable() override {
    return linear_program_.CreateNewVariable().value();
  }
  bool SetVariableBounds(int index, int64_t lower_bound,
                         int64_t upper_bound) override {
    DCHECK_GE(lower_bound, 0);
    // When variable upper bounds are greater than this threshold, precision
    // issues arise in GLOP. In this case we are just going to suppose that
    // these high bound values are infinite and not set the upper bound.
    const int64_t kMaxValue = 1e10;
    const double lp_min = lower_bound;
    const double lp_max =
        (upper_bound > kMaxValue) ? glop::kInfinity : upper_bound;
    if (lp_min <= lp_max) {
      linear_program_.SetVariableBounds(glop::ColIndex(index), lp_min, lp_max);
      return true;
    }
    // The linear_program would not be feasible, and it cannot handle the
    // lp_min > lp_max case, so we must detect infeasibility here.
    return false;
  }
  void SetVariableDisjointBounds(int index, const std::vector<int64_t>& starts,
                                 const std::vector<int64_t>& ends) override {
    // TODO(user): Investigate if we can avoid rebuilding the interval list
    // each time (we could keep a reference to the forbidden interval list in
    // RoutingDimension but we would need to store cumul offsets and use them
    // when checking intervals).
    allowed_intervals_[index] =
        absl::make_unique<SortedDisjointIntervalList>(starts, ends);
  }
  int64_t GetVariableLowerBound(int index) const override {
    return linear_program_.variable_lower_bounds()[glop::ColIndex(index)];
  }
  int64_t GetVariableUpperBound(int index) const override {
    const double upper_bound =
        linear_program_.variable_upper_bounds()[glop::ColIndex(index)];
    DCHECK_GE(upper_bound, 0);
    return upper_bound == glop::kInfinity ? std::numeric_limits<int64_t>::max()
                                          : static_cast<int64_t>(upper_bound);
  }
  void SetObjectiveCoefficient(int index, double coefficient) override {
    linear_program_.SetObjectiveCoefficient(glop::ColIndex(index), coefficient);
  }
  double GetObjectiveCoefficient(int index) const override {
    return linear_program_.objective_coefficients()[glop::ColIndex(index)];
  }
  void ClearObjective() override {
    for (glop::ColIndex i(0); i < linear_program_.num_variables(); ++i) {
      linear_program_.SetObjectiveCoefficient(i, 0);
    }
  }
  int NumVariables() const override {
    return linear_program_.num_variables().value();
  }
  int CreateNewConstraint(int64_t lower_bound, int64_t upper_bound) override {
    const glop::RowIndex ct = linear_program_.CreateNewConstraint();
    linear_program_.SetConstraintBounds(
        ct,
        (lower_bound == std::numeric_limits<int64_t>::min()) ? -glop::kInfinity
                                                             : lower_bound,
        (upper_bound == std::numeric_limits<int64_t>::max()) ? glop::kInfinity
                                                             : upper_bound);
    return ct.value();
  }
  void SetCoefficient(int ct, int index, double coefficient) override {
    linear_program_.SetCoefficient(glop::RowIndex(ct), glop::ColIndex(index),
                                   coefficient);
  }
  bool IsCPSATSolver() override { return false; }
  void AddObjectiveConstraint() override {
    const int ct = CreateNewConstraint(0, GetObjectiveValue());
    for (int variable = 0; variable < NumVariables(); variable++) {
      const double coefficient = GetObjectiveCoefficient(variable);
      if (coefficient != 0) {
        SetCoefficient(ct, variable, coefficient);
      }
    }
  }
  void AddMaximumConstraint(int max_var, std::vector<int> vars) override {}
  void AddProductConstraint(int product_var, std::vector<int> vars) override {}
  void SetEnforcementLiteral(int ct, int condition) override{};
  DimensionSchedulingStatus Solve(absl::Duration duration_limit) override {
    lp_solver_.GetMutableParameters()->set_max_time_in_seconds(
        absl::ToDoubleSeconds(duration_limit));

    // Because we construct the lp one constraint at a time and we never call
    // SetCoefficient() on the same variable twice for a constraint, we know
    // that the columns do not contain duplicates and are already ordered by
    // constraint so we do not need to call linear_program->CleanUp() which can
    // be costly. Note that the assumptions are DCHECKed() in the call below.
    linear_program_.NotifyThatColumnsAreClean();
    VLOG(2) << linear_program_.Dump();
    const glop::ProblemStatus status = lp_solver_.Solve(linear_program_);
    if (status != glop::ProblemStatus::OPTIMAL &&
        status != glop::ProblemStatus::IMPRECISE) {
      return DimensionSchedulingStatus::INFEASIBLE;
    }
    if (is_relaxation_) {
      return DimensionSchedulingStatus::RELAXED_OPTIMAL_ONLY;
    }
    for (const auto& allowed_interval : allowed_intervals_) {
      const double value_double = GetValue(allowed_interval.first);
      const int64_t value =
          (value_double >= std::numeric_limits<int64_t>::max())
              ? std::numeric_limits<int64_t>::max()
              : MathUtil::FastInt64Round(value_double);
      const SortedDisjointIntervalList* const interval_list =
          allowed_interval.second.get();
      const auto it = interval_list->FirstIntervalGreaterOrEqual(value);
      if (it == interval_list->end() || value < it->start) {
        return DimensionSchedulingStatus::RELAXED_OPTIMAL_ONLY;
      }
    }
    return DimensionSchedulingStatus::OPTIMAL;
  }
  int64_t GetObjectiveValue() const override {
    return MathUtil::FastInt64Round(lp_solver_.GetObjectiveValue());
  }
  double GetValue(int index) const override {
    return lp_solver_.variable_values()[glop::ColIndex(index)];
  }
  bool SolutionIsInteger() const override {
    return linear_program_.SolutionIsInteger(lp_solver_.variable_values(),
                                             /*absolute_tolerance*/ 1e-3);
  }

 private:
  const bool is_relaxation_;
  glop::LinearProgram linear_program_;
  glop::LPSolver lp_solver_;
  absl::flat_hash_map<int, std::unique_ptr<SortedDisjointIntervalList>>
      allowed_intervals_;
};

class RoutingCPSatWrapper : public RoutingLinearSolverWrapper {
 public:
  RoutingCPSatWrapper() {
    parameters_.set_num_search_workers(1);
    // Keeping presolve but with 0 iterations; as of 11/2019 it is
    // significantly faster than both full presolve and no presolve.
    parameters_.set_cp_model_presolve(true);
    parameters_.set_max_presolve_iterations(0);
    parameters_.set_catch_sigint_signal(false);
    parameters_.set_mip_max_bound(1e8);
    parameters_.set_search_branching(sat::SatParameters::LP_SEARCH);
  }
  ~RoutingCPSatWrapper() override {}
  void Clear() override {
    model_.Clear();
    response_.Clear();
    objective_coefficients_.clear();
  }
  int CreateNewPositiveVariable() override {
    const int index = model_.variables_size();
    sat::IntegerVariableProto* const variable = model_.add_variables();
    variable->add_domain(0);
    variable->add_domain(static_cast<int64_t>(parameters_.mip_max_bound()));
    return index;
  }
  bool SetVariableBounds(int index, int64_t lower_bound,
                         int64_t upper_bound) override {
    DCHECK_GE(lower_bound, 0);
    const int64_t capped_upper_bound =
        std::min<int64_t>(upper_bound, parameters_.mip_max_bound());
    if (lower_bound > capped_upper_bound) return false;
    sat::IntegerVariableProto* const variable = model_.mutable_variables(index);
    variable->set_domain(0, lower_bound);
    variable->set_domain(1, capped_upper_bound);
    return true;
  }
  void SetVariableDisjointBounds(int index, const std::vector<int64_t>& starts,
                                 const std::vector<int64_t>& ends) override {
    DCHECK_EQ(starts.size(), ends.size());
    const int ct = CreateNewConstraint(1, 1);
    for (int i = 0; i < starts.size(); ++i) {
      const int variable = CreateNewPositiveVariable();
      SetVariableBounds(variable, 0, 1);
      SetCoefficient(ct, variable, 1);
      const int window_ct = CreateNewConstraint(starts[i], ends[i]);
      SetCoefficient(window_ct, index, 1);
      model_.mutable_constraints(window_ct)->add_enforcement_literal(variable);
    }
  }
  int64_t GetVariableLowerBound(int index) const override {
    return model_.variables(index).domain(0);
  }
  int64_t GetVariableUpperBound(int index) const override {
    const auto& domain = model_.variables(index).domain();
    return domain[domain.size() - 1];
  }
  void SetObjectiveCoefficient(int index, double coefficient) override {
    if (index >= objective_coefficients_.size()) {
      objective_coefficients_.resize(index + 1, 0);
    }
    objective_coefficients_[index] = coefficient;
    sat::FloatObjectiveProto* const objective =
        model_.mutable_floating_point_objective();
    objective->add_vars(index);
    objective->add_coeffs(coefficient);
  }
  double GetObjectiveCoefficient(int index) const override {
    return (index < objective_coefficients_.size())
               ? objective_coefficients_[index]
               : 0;
  }
  void ClearObjective() override {
    model_.mutable_floating_point_objective()->Clear();
  }
  int NumVariables() const override { return model_.variables_size(); }
  int CreateNewConstraint(int64_t lower_bound, int64_t upper_bound) override {
    sat::LinearConstraintProto* const ct =
        model_.add_constraints()->mutable_linear();
    ct->add_domain(lower_bound);
    ct->add_domain(upper_bound);
    return model_.constraints_size() - 1;
  }
  void SetCoefficient(int ct_index, int index, double coefficient) override {
    sat::LinearConstraintProto* const ct =
        model_.mutable_constraints(ct_index)->mutable_linear();
    ct->add_vars(index);
    const int64_t integer_coefficient = coefficient;
    ct->add_coeffs(integer_coefficient);
  }
  bool IsCPSATSolver() override { return true; }
  void AddObjectiveConstraint() override {
    // Scale the objective to get its integer representation.
    const sat::FloatObjectiveProto& fp_objective =
        model_.floating_point_objective();
    std::vector<std::pair<int, double>> terms;
    for (int i = 0; i < fp_objective.vars_size(); ++i) {
      terms.push_back({fp_objective.vars(i), fp_objective.coeffs(i)});
    }
    SolverLogger logger;
    const bool status =
        sat::ScaleAndSetObjective(parameters_, terms, fp_objective.offset(),
                                  fp_objective.maximize(), &model_, &logger);
    DCHECK(status);
    const sat::CpObjectiveProto& objective = model_.objective();
    int64_t activity = 0;
    for (int i = 0; i < objective.vars_size(); ++i) {
      activity += response_.solution(objective.vars(i)) * objective.coeffs(i);
    }
    const int ct = CreateNewConstraint(0, activity);
    for (int i = 0; i < objective.vars_size(); ++i) {
      SetCoefficient(ct, objective.vars(i), objective.coeffs(i));
    }
    model_.clear_objective();
  }
  void AddMaximumConstraint(int max_var, std::vector<int> vars) override {
    sat::LinearArgumentProto* const ct =
        model_.add_constraints()->mutable_lin_max();
    ct->mutable_target()->add_vars(max_var);
    ct->mutable_target()->add_coeffs(1);
    for (const int var : vars) {
      sat::LinearExpressionProto* const expr = ct->add_exprs();
      expr->add_vars(var);
      expr->add_coeffs(1);
    }
  }
  void AddProductConstraint(int product_var, std::vector<int> vars) override {
    sat::LinearArgumentProto* const ct =
        model_.add_constraints()->mutable_int_prod();
    ct->mutable_target()->add_vars(product_var);
    ct->mutable_target()->add_coeffs(1);
    for (const int var : vars) {
      sat::LinearExpressionProto* expr = ct->add_exprs();
      expr->add_vars(var);
      expr->add_coeffs(1);
    }
  }
  void SetEnforcementLiteral(int ct, int condition) override {
    DCHECK_LT(ct, model_.constraints_size());
    model_.mutable_constraints(ct)->add_enforcement_literal(condition);
  }
  DimensionSchedulingStatus Solve(absl::Duration duration_limit) override {
    parameters_.set_max_time_in_seconds(absl::ToDoubleSeconds(duration_limit));
    VLOG(2) << model_.DebugString();
    if (hint_.vars_size() == model_.variables_size()) {
      *model_.mutable_solution_hint() = hint_;
    }
    sat::Model model;
    model.Add(sat::NewSatParameters(parameters_));
    response_ = sat::SolveCpModel(model_, &model);
    VLOG(2) << response_.DebugString();
    if (response_.status() == sat::CpSolverStatus::OPTIMAL ||
        (response_.status() == sat::CpSolverStatus::FEASIBLE &&
         !model_.has_floating_point_objective())) {
      hint_.Clear();
      for (int i = 0; i < response_.solution_size(); ++i) {
        hint_.add_vars(i);
        hint_.add_values(response_.solution(i));
      }
      return DimensionSchedulingStatus::OPTIMAL;
    }
    return DimensionSchedulingStatus::INFEASIBLE;
  }
  int64_t GetObjectiveValue() const override {
    return MathUtil::FastInt64Round(response_.objective_value());
  }
  double GetValue(int index) const override {
    return response_.solution(index);
  }
  bool SolutionIsInteger() const override { return true; }

 private:
  sat::CpModelProto model_;
  sat::CpSolverResponse response_;
  sat::SatParameters parameters_;
  std::vector<double> objective_coefficients_;
  sat::PartialVariableAssignment hint_;
};

// Utility class used in Local/GlobalDimensionCumulOptimizer to set the linear
// solver constraints and solve the problem.
class DimensionCumulOptimizerCore {
 public:
  DimensionCumulOptimizerCore(const RoutingDimension* dimension,
                              bool use_precedence_propagator);

  // In the OptimizeSingleRoute() and Optimize() methods, if both "cumul_values"
  // and "cost" parameters are null, we don't optimize the cost and stop at the
  // first feasible solution in the linear solver (since in this case only
  // feasibility is of interest).
  DimensionSchedulingStatus OptimizeSingleRoute(
      int vehicle, const std::function<int64_t(int64_t)>& next_accessor,
      RoutingLinearSolverWrapper* solver, std::vector<int64_t>* cumul_values,
      std::vector<int64_t>* break_values, int64_t* cost, int64_t* transit_cost,
      bool clear_lp = true);

  std::vector<DimensionSchedulingStatus> OptimizeSingleRouteWithResources(
      int vehicle, const std::function<int64_t(int64_t)>& next_accessor,
      const std::vector<RoutingModel::ResourceGroup::Resource>& resources,
      const std::vector<int>& resource_indices, bool optimize_vehicle_costs,
      RoutingLinearSolverWrapper* solver,
      std::vector<int64_t>* costs_without_transits,
      std::vector<std::vector<int64_t>>* cumul_values,
      std::vector<std::vector<int64_t>>* break_values, bool clear_lp = true);

  DimensionSchedulingStatus Optimize(
      const std::function<int64_t(int64_t)>& next_accessor,
      RoutingLinearSolverWrapper* solver, std::vector<int64_t>* cumul_values,
      std::vector<int64_t>* break_values,
      std::vector<std::vector<int>>* resource_indices_per_group, int64_t* cost,
      int64_t* transit_cost, bool clear_lp = true);

  DimensionSchedulingStatus OptimizeAndPack(
      const std::function<int64_t(int64_t)>& next_accessor,
      RoutingLinearSolverWrapper* solver, std::vector<int64_t>* cumul_values,
      std::vector<int64_t>* break_values,
      std::vector<std::vector<int>>* resource_indices_per_group);

  DimensionSchedulingStatus OptimizeAndPackSingleRoute(
      int vehicle, const std::function<int64_t(int64_t)>& next_accessor,
      const RoutingModel::ResourceGroup::Resource* resource,
      RoutingLinearSolverWrapper* solver, std::vector<int64_t>* cumul_values,
      std::vector<int64_t>* break_values);

  const RoutingDimension* dimension() const { return dimension_; }

 private:
  // Initializes the containers and given solver. Must be called prior to
  // setting any contraints and solving.
  void InitOptimizer(RoutingLinearSolverWrapper* solver);

  // Computes the minimum/maximum of cumuls for nodes on "route", and sets them
  // in current_route_[min|max]_cumuls_ respectively.
  // If the propagator_ is not null, uses the bounds tightened by the
  // propagator.
  // Otherwise, the bounds are computed by going over the nodes on the route
  // using the CP bounds, and the fixed transits are used to tighten them.
  bool ComputeRouteCumulBounds(const std::vector<int64_t>& route,
                               const std::vector<int64_t>& fixed_transits,
                               int64_t cumul_offset);

  // Sets the constraints for all nodes on "vehicle"'s route according to
  // "next_accessor". If optimize_costs is true, also sets the objective
  // coefficients for the LP.
  // Returns false if some infeasibility was detected, true otherwise.
  bool SetRouteCumulConstraints(
      int vehicle, const std::function<int64_t(int64_t)>& next_accessor,
      int64_t cumul_offset, bool optimize_costs,
      RoutingLinearSolverWrapper* solver, int64_t* route_transit_cost,
      int64_t* route_cost_offset);

  // Sets the global constraints on the dimension, and adds global objective
  // cost coefficients if optimize_costs is true.
  // NOTE: When called, the call to this function MUST come after
  // SetRouteCumulConstraints() has been called on all routes, so that
  // index_to_cumul_variable_ and min_start/max_end_cumul_ are correctly
  // initialized.
  // Returns false if some infeasibility was detected, true otherwise.
  bool SetGlobalConstraints(
      const std::function<int64_t(int64_t)>& next_accessor,
      int64_t cumul_offset, bool optimize_costs,
      RoutingLinearSolverWrapper* solver);

  void SetValuesFromLP(const std::vector<int>& lp_variables, int64_t offset,
                       RoutingLinearSolverWrapper* solver,
                       std::vector<int64_t>* lp_values) const;

  void SetResourceIndices(
      RoutingLinearSolverWrapper* solver,
      std::vector<std::vector<int>>* resource_indices_per_group) const;

  // This function packs the routes of the given vehicles while keeping the cost
  // of the LP lower than its current (supposed optimal) objective value.
  // It does so by setting the current objective variables' coefficient to 0 and
  // setting the coefficient of the route ends to 1, to first minimize the route
  // ends' cumuls, and then maximizes the starts' cumuls without increasing the
  // ends.
  DimensionSchedulingStatus PackRoutes(std::vector<int> vehicles,
                                       RoutingLinearSolverWrapper* solver);

  std::unique_ptr<CumulBoundsPropagator> propagator_;
  std::vector<int64_t> current_route_min_cumuls_;
  std::vector<int64_t> current_route_max_cumuls_;
  const RoutingDimension* const dimension_;
  // Scheduler variables for current route cumuls and for all nodes cumuls.
  std::vector<int> current_route_cumul_variables_;
  std::vector<int> index_to_cumul_variable_;
  // Scheduler variables for current route breaks and all vehicle breaks.
  // There are two variables for each break: start and end.
  // current_route_break_variables_ has variables corresponding to
  // break[0] start, break[0] end, break[1] start, break[1] end, etc.
  std::vector<int> current_route_break_variables_;
  // Vector all_break_variables contains the break variables of all vehicles,
  // in the same format as current_route_break_variables.
  // It is the concatenation of break variables of vehicles in [0, #vehicles).
  std::vector<int> all_break_variables_;
  // Allows to retrieve break variables of a given vehicle: those go from
  // all_break_variables_[vehicle_to_all_break_variables_offset_[vehicle]] to
  // all_break_variables[vehicle_to_all_break_variables_offset_[vehicle+1]-1].
  std::vector<int> vehicle_to_all_break_variables_offset_;
  // The following vector contains indices of resource-to-vehicle assignment
  // variables. For every resource group, stores indices of
  // num_resources*num_vehicles boolean variables indicating whether resource #r
  // is assigned to vehicle #v.
  std::vector<std::vector<int>>
      resource_group_to_resource_to_vehicle_assignment_variables_;

  int max_end_cumul_;
  int min_start_cumul_;
  std::vector<std::pair<int64_t, int64_t>>
      visited_pickup_delivery_indices_for_pair_;
};

// Class used to compute optimal values for dimension cumuls of routes,
// minimizing cumul soft lower and upper bound costs, and vehicle span costs of
// a route.
// In its methods, next_accessor is a callback returning the next node of a
// given node on a route.
class LocalDimensionCumulOptimizer {
 public:
  LocalDimensionCumulOptimizer(
      const RoutingDimension* dimension,
      RoutingSearchParameters::SchedulingSolver solver_type);

  // If feasible, computes the optimal cost of the route performed by a vehicle,
  // minimizing cumul soft lower and upper bound costs and vehicle span costs,
  // and stores it in "optimal_cost" (if not null).
  // Returns true iff the route respects all constraints.
  DimensionSchedulingStatus ComputeRouteCumulCost(
      int vehicle, const std::function<int64_t(int64_t)>& next_accessor,
      int64_t* optimal_cost);

  // Same as ComputeRouteCumulCost, but the cost computed does not contain
  // the part of the vehicle span cost due to fixed transits.
  DimensionSchedulingStatus ComputeRouteCumulCostWithoutFixedTransits(
      int vehicle, const std::function<int64_t(int64_t)>& next_accessor,
      int64_t* optimal_cost_without_transits);

  std::vector<DimensionSchedulingStatus>
  ComputeRouteCumulCostsForResourcesWithoutFixedTransits(
      int vehicle, const std::function<int64_t(int64_t)>& next_accessor,
      const std::vector<RoutingModel::ResourceGroup::Resource>& resources,
      const std::vector<int>& resource_indices, bool optimize_vehicle_costs,
      std::vector<int64_t>* optimal_costs_without_transits,
      std::vector<std::vector<int64_t>>* optimal_cumuls,
      std::vector<std::vector<int64_t>>* optimal_breaks);

  // If feasible, computes the optimal values for cumul and break variables
  // of the route performed by a vehicle, minimizing cumul soft lower, upper
  // bound costs and vehicle span costs, stores them in "optimal_cumuls"
  // (if not null), and optimal_breaks, and returns true.
  // Returns false if the route is not feasible.
  DimensionSchedulingStatus ComputeRouteCumuls(
      int vehicle, const std::function<int64_t(int64_t)>& next_accessor,
      std::vector<int64_t>* optimal_cumuls,
      std::vector<int64_t>* optimal_breaks);

  // Similar to ComputeRouteCumuls, but also tries to pack the cumul values on
  // the route, such that the cost remains the same, the cumul of route end is
  // minimized, and then the cumul of the start of the route is maximized.
  // If 'resource' is non-null, the packed route must also respect its start/end
  // time window.
  DimensionSchedulingStatus ComputePackedRouteCumuls(
      int vehicle, const std::function<int64_t(int64_t)>& next_accessor,
      const RoutingModel::ResourceGroup::Resource* resource,
      std::vector<int64_t>* packed_cumuls, std::vector<int64_t>* packed_breaks);

  const RoutingDimension* dimension() const {
    return optimizer_core_.dimension();
  }

 private:
  std::vector<std::unique_ptr<RoutingLinearSolverWrapper>> solver_;
  DimensionCumulOptimizerCore optimizer_core_;
};

class GlobalDimensionCumulOptimizer {
 public:
  GlobalDimensionCumulOptimizer(
      const RoutingDimension* dimension,
      RoutingSearchParameters::SchedulingSolver solver_type);
  // If feasible, computes the optimal cost of the entire model with regards to
  // the optimizer_core_'s dimension costs, minimizing cumul soft lower/upper
  // bound costs and vehicle/global span costs, and stores it in "optimal_cost"
  // (if not null).
  // Returns true iff all the constraints can be respected.
  DimensionSchedulingStatus ComputeCumulCostWithoutFixedTransits(
      const std::function<int64_t(int64_t)>& next_accessor,
      int64_t* optimal_cost_without_transits);
  // If feasible, computes the optimal values for cumul, break and resource
  // variables, minimizing cumul soft lower/upper bound costs and vehicle/global
  // span costs, stores them in "optimal_cumuls" (if not null), "optimal_breaks"
  // and "optimal_resource_indices_per_group", and returns true.
  // Returns false if the routes are not feasible.
  DimensionSchedulingStatus ComputeCumuls(
      const std::function<int64_t(int64_t)>& next_accessor,
      std::vector<int64_t>* optimal_cumuls,
      std::vector<int64_t>* optimal_breaks,
      std::vector<std::vector<int>>* optimal_resource_indices_per_group);

  // Similar to ComputeCumuls, but also tries to pack the cumul values on all
  // routes, such that the cost remains the same, the cumuls of route ends are
  // minimized, and then the cumuls of the starts of the routes are maximized.
  DimensionSchedulingStatus ComputePackedCumuls(
      const std::function<int64_t(int64_t)>& next_accessor,
      std::vector<int64_t>* packed_cumuls, std::vector<int64_t>* packed_breaks,
      std::vector<std::vector<int>>* resource_indices_per_group);

  const RoutingDimension* dimension() const {
    return optimizer_core_.dimension();
  }

 private:
  std::unique_ptr<RoutingLinearSolverWrapper> solver_;
  DimensionCumulOptimizerCore optimizer_core_;
};

class ResourceAssignmentOptimizer {
 public:
  ResourceAssignmentOptimizer(const RoutingModel::ResourceGroup* resource_group,
                              LocalDimensionCumulOptimizer* optimizer,
                              LocalDimensionCumulOptimizer* mp_optimizer);

  // Returns the cost resulting from the min-cost assignment of resources to
  // (used) vehicles, or -1 if the assignment is impossible.
  // For each vehicle v and resource r, the cost of assigning r to v is equal to
  // - primary_vehicle_to_resource_assignment_costs[v][r] if
  //   primary_vehicle_to_resource_assignment_costs[v] is not empty,
  // - secondary_vehicle_to_resource_assignment_costs[v][r] otherwise
  //   (secondary_vehicle_to_resource_assignment_costs[v] can never be empty).
  // If non-null, 'resource_indices' contains the index of the resource assigned
  // to each vehicle.
  int64_t ComputeBestAssignmentCost(
      const std::vector<std::vector<int64_t>>&
          primary_vehicle_to_resource_assignment_costs,
      const std::vector<std::vector<int64_t>>&
          secondary_vehicle_to_resource_assignment_costs,
      const std::function<bool(int)>& use_primary_for_vehicle,
      std::vector<int>* resource_indices) const;

  // Computes the vehicle-to-resource assignment costs for the given vehicle to
  // all resources in the group, and sets these costs in assignment_costs (if
  // non-null).
  // optimize_vehicle_costs indicates if the costs should be optimized or if
  // we merely care about feasibility (cost of 0) and infeasibility (cost of -1)
  // of the assignments.
  // The cumul and break values corresponding to the assignment of each resource
  // are also set in cumul_values and break_values, if non-null.
  bool ComputeAssignmentCostsForVehicle(
      int v, const std::function<int64_t(int64_t)>& next_accessor,
      bool optimize_vehicle_costs, std::vector<int64_t>* assignment_costs,
      std::vector<std::vector<int64_t>>* cumul_values,
      std::vector<std::vector<int64_t>>* break_values);

  const RoutingDimension* const dimension() const {
    return optimizer_.dimension();
  }

 private:
  LocalDimensionCumulOptimizer& optimizer_;
  LocalDimensionCumulOptimizer& mp_optimizer_;
  const RoutingModel& model_;
  const RoutingModel::ResourceGroup& resource_group_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_LP_SCHEDULING_H_
