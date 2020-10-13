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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_LP_SCHEDULING_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_LP_SCHEDULING_H_

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "ortools/base/mathutil.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/util/saturated_arithmetic.h"

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
  bool PropagateCumulBounds(const std::function<int64(int64)>& next_accessor,
                            int64 cumul_offset);

  int64 CumulMin(int index) const {
    return propagated_bounds_[PositiveNode(index)];
  }

  int64 CumulMax(int index) const {
    const int64 negated_upper_bound = propagated_bounds_[NegativeNode(index)];
    return negated_upper_bound == kint64min ? kint64max : -negated_upper_bound;
  }

  const RoutingDimension& dimension() const { return dimension_; }

 private:
  // An arc "tail --offset--> head" represents the relation
  // tail + offset <= head.
  // As arcs are stored by tail, we don't store it in the struct.
  struct ArcInfo {
    int head;
    int64 offset;
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
  void AddArcs(int first_index, int second_index, int64 offset);

  bool InitializeArcsAndBounds(const std::function<int64(int64)>& next_accessor,
                               int64 cumul_offset);

  bool UpdateCurrentLowerBoundOfNode(int node, int64 new_lb, int64 offset);

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
  const int64 num_nodes_;

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
  std::vector<int64> propagated_bounds_;

  // Vector used in DisassembleSubtree() to avoid memory reallocation.
  std::vector<int> tmp_dfs_stack_;

  // Used to store the pickup/delivery pairs encountered on the routes.
  std::vector<std::pair<int64, int64>>
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
  virtual bool SetVariableBounds(int index, int64 lower_bound,
                                 int64 upper_bound) = 0;
  virtual void SetVariableDisjointBounds(int index,
                                         const std::vector<int64>& starts,
                                         const std::vector<int64>& ends) = 0;
  virtual int64 GetVariableLowerBound(int index) const = 0;
  virtual void SetObjectiveCoefficient(int index, double coefficient) = 0;
  virtual double GetObjectiveCoefficient(int index) const = 0;
  virtual void ClearObjective() = 0;
  virtual int NumVariables() const = 0;
  virtual int CreateNewConstraint(int64 lower_bound, int64 upper_bound) = 0;
  virtual void SetCoefficient(int ct, int index, double coefficient) = 0;
  virtual bool IsCPSATSolver() = 0;
  virtual void AddMaximumConstraint(int max_var, std::vector<int> vars) = 0;
  virtual void AddProductConstraint(int product_var, std::vector<int> vars) = 0;
  virtual void SetEnforcementLiteral(int ct, int condition) = 0;
  virtual DimensionSchedulingStatus Solve(absl::Duration duration_limit) = 0;
  virtual int64 GetObjectiveValue() const = 0;
  virtual double GetValue(int index) const = 0;
  virtual bool SolutionIsInteger() const = 0;

  // Adds a variable with bounds [lower_bound, upper_bound].
  int AddVariable(int64 lower_bound, int64 upper_bound) {
    CHECK_LE(lower_bound, upper_bound);
    const int variable = CreateNewPositiveVariable();
    SetVariableBounds(variable, lower_bound, upper_bound);
    return variable;
  }
  // Adds a linear constraint, enforcing
  // lower_bound <= sum variable * coeff <= upper_bound,
  // and returns the identifier of that constraint.
  int AddLinearConstraint(
      int64 lower_bound, int64 upper_bound,
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
      int64 lower_bound, int64 upper_bound,
      const std::vector<std::pair<int, double>>& weighted_variables) {
    const int reification_ct = AddLinearConstraint(1, 1, {});
    if (kint64min < lower_bound) {
      const int under_lower_bound = AddVariable(0, 1);
      SetCoefficient(reification_ct, under_lower_bound, 1);
      const int under_lower_bound_ct =
          AddLinearConstraint(kint64min, lower_bound - 1, weighted_variables);
      SetEnforcementLiteral(under_lower_bound_ct, under_lower_bound);
    }
    if (upper_bound < kint64max) {
      const int above_upper_bound = AddVariable(0, 1);
      SetCoefficient(reification_ct, above_upper_bound, 1);
      const int above_upper_bound_ct =
          AddLinearConstraint(upper_bound + 1, kint64max, weighted_variables);
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
  explicit RoutingGlopWrapper(const glop::GlopParameters& parameters) {
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
  bool SetVariableBounds(int index, int64 lower_bound,
                         int64 upper_bound) override {
    DCHECK_GE(lower_bound, 0);
    // When variable upper bounds are greater than this threshold, precision
    // issues arise in GLOP. In this case we are just going to suppose that
    // these high bound values are infinite and not set the upper bound.
    const int64 kMaxValue = 1e10;
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
  void SetVariableDisjointBounds(int index, const std::vector<int64>& starts,
                                 const std::vector<int64>& ends) override {
    // TODO(user): Investigate if we can avoid rebuilding the interval list
    // each time (we could keep a reference to the forbidden interval list in
    // RoutingDimension but we would need to store cumul offsets and use them
    // when checking intervals).
    allowed_intervals_[index] =
        absl::make_unique<SortedDisjointIntervalList>(starts, ends);
  }
  int64 GetVariableLowerBound(int index) const override {
    return linear_program_.variable_lower_bounds()[glop::ColIndex(index)];
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
  int CreateNewConstraint(int64 lower_bound, int64 upper_bound) override {
    const glop::RowIndex ct = linear_program_.CreateNewConstraint();
    linear_program_.SetConstraintBounds(
        ct, (lower_bound == kint64min) ? -glop::kInfinity : lower_bound,
        (upper_bound == kint64max) ? glop::kInfinity : upper_bound);
    return ct.value();
  }
  void SetCoefficient(int ct, int index, double coefficient) override {
    linear_program_.SetCoefficient(glop::RowIndex(ct), glop::ColIndex(index),
                                   coefficient);
  }
  bool IsCPSATSolver() override { return false; }
  void AddMaximumConstraint(int max_var, std::vector<int> vars) override{};
  void AddProductConstraint(int product_var, std::vector<int> vars) override{};
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
      linear_program_.Clear();
      return DimensionSchedulingStatus::INFEASIBLE;
    }
    for (const auto& allowed_interval : allowed_intervals_) {
      const double value_double = GetValue(allowed_interval.first);
      const int64 value = (value_double >= kint64max)
                              ? kint64max
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
  int64 GetObjectiveValue() const override {
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
  glop::LinearProgram linear_program_;
  glop::LPSolver lp_solver_;
  absl::flat_hash_map<int, std::unique_ptr<SortedDisjointIntervalList>>
      allowed_intervals_;
};

class RoutingCPSatWrapper : public RoutingLinearSolverWrapper {
 public:
  RoutingCPSatWrapper() : objective_offset_(0), first_constraint_to_offset_(0) {
    parameters_.set_num_search_workers(1);
    // Keeping presolve but with 0 iterations; as of 11/2019 it is
    // significantly faster than both full presolve and no presolve.
    parameters_.set_cp_model_presolve(true);
    parameters_.set_max_presolve_iterations(0);
    parameters_.set_catch_sigint_signal(false);
    parameters_.set_mip_max_bound(1e8);
  }
  ~RoutingCPSatWrapper() override {}
  void Clear() override {
    model_.Clear();
    response_.Clear();
    objective_coefficients_.clear();
    objective_offset_ = 0;
    variable_offset_.clear();
    constraint_offset_.clear();
    first_constraint_to_offset_ = 0;
  }
  int CreateNewPositiveVariable() override {
    const int index = model_.variables_size();
    if (index >= variable_offset_.size()) {
      variable_offset_.resize(index + 1, 0);
    }
    sat::IntegerVariableProto* const variable = model_.add_variables();
    variable->add_domain(0);
    variable->add_domain(static_cast<int64>(parameters_.mip_max_bound()));
    return index;
  }
  bool SetVariableBounds(int index, int64 lower_bound,
                         int64 upper_bound) override {
    DCHECK_GE(lower_bound, 0);
    // TODO(user): Find whether there is a way to make the offsetting
    // system work with other CP-SAT constraints than linear constraints.
    // variable_offset_[index] = lower_bound;
    variable_offset_[index] = 0;
    const int64 offset_upper_bound =
        std::min<int64>(CapSub(upper_bound, variable_offset_[index]),
                        parameters_.mip_max_bound());
    const int64 offset_lower_bound =
        CapSub(lower_bound, variable_offset_[index]);
    if (offset_lower_bound > offset_upper_bound) return false;
    sat::IntegerVariableProto* const variable = model_.mutable_variables(index);
    variable->set_domain(0, offset_lower_bound);
    variable->set_domain(1, offset_upper_bound);
    return true;
  }
  void SetVariableDisjointBounds(int index, const std::vector<int64>& starts,
                                 const std::vector<int64>& ends) override {
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
  int64 GetVariableLowerBound(int index) const override {
    return CapAdd(model_.variables(index).domain(0), variable_offset_[index]);
  }
  void SetObjectiveCoefficient(int index, double coefficient) override {
    // TODO(user): Check variable bounds are never set after setting the
    // objective coefficient.
    if (index >= objective_coefficients_.size()) {
      objective_coefficients_.resize(index + 1, 0);
    }
    objective_coefficients_[index] = coefficient;
    sat::CpObjectiveProto* const objective = model_.mutable_objective();
    objective->add_vars(index);
    objective->add_coeffs(coefficient);
    objective_offset_ += coefficient * variable_offset_[index];
  }
  double GetObjectiveCoefficient(int index) const override {
    return (index < objective_coefficients_.size())
               ? objective_coefficients_[index]
               : 0;
  }
  void ClearObjective() override {
    model_.mutable_objective()->Clear();
    objective_offset_ = 0;
  }
  int NumVariables() const override { return model_.variables_size(); }
  int CreateNewConstraint(int64 lower_bound, int64 upper_bound) override {
    const int ct_index = model_.constraints_size();
    if (ct_index >= constraint_offset_.size()) {
      constraint_offset_.resize(ct_index + 1, 0);
    }
    sat::LinearConstraintProto* const ct =
        model_.add_constraints()->mutable_linear();
    ct->add_domain(lower_bound);
    ct->add_domain(upper_bound);
    return ct_index;
  }
  void SetCoefficient(int ct_index, int index, double coefficient) override {
    // TODO(user): Check variable bounds are never set after setting the
    // variable coefficient.
    sat::LinearConstraintProto* const ct =
        model_.mutable_constraints(ct_index)->mutable_linear();
    ct->add_vars(index);
    ct->add_coeffs(coefficient);
    constraint_offset_[ct_index] =
        CapAdd(constraint_offset_[ct_index],
               CapProd(variable_offset_[index], coefficient));
  }
  bool IsCPSATSolver() override { return true; }
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
    sat::IntegerArgumentProto* const ct =
        model_.add_constraints()->mutable_int_prod();
    ct->set_target(product_var);
    for (const int var : vars) {
      ct->add_vars(var);
    }
  }
  void SetEnforcementLiteral(int ct, int condition) override {
    DCHECK_LT(ct, constraint_offset_.size());
    model_.mutable_constraints(ct)->add_enforcement_literal(condition);
  }
  DimensionSchedulingStatus Solve(absl::Duration duration_limit) override {
    // Set constraint offsets
    for (int ct_index = first_constraint_to_offset_;
         ct_index < constraint_offset_.size(); ++ct_index) {
      if (!model_.mutable_constraints(ct_index)->has_linear()) continue;
      sat::LinearConstraintProto* const ct =
          model_.mutable_constraints(ct_index)->mutable_linear();
      ct->set_domain(0, CapSub(ct->domain(0), constraint_offset_[ct_index]));
      ct->set_domain(1, CapSub(ct->domain(1), constraint_offset_[ct_index]));
    }
    first_constraint_to_offset_ = constraint_offset_.size();
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
         !model_.has_objective())) {
      hint_.Clear();
      for (int i = 0; i < response_.solution_size(); ++i) {
        hint_.add_vars(i);
        hint_.add_values(response_.solution(i));
      }
      return DimensionSchedulingStatus::OPTIMAL;
    }
    return DimensionSchedulingStatus::INFEASIBLE;
  }
  int64 GetObjectiveValue() const override {
    return MathUtil::FastInt64Round(response_.objective_value() +
                                    objective_offset_);
  }
  double GetValue(int index) const override {
    return response_.solution(index) + variable_offset_[index];
  }
  bool SolutionIsInteger() const override { return true; }

 private:
  sat::CpModelProto model_;
  sat::CpSolverResponse response_;
  sat::SatParameters parameters_;
  std::vector<double> objective_coefficients_;
  double objective_offset_;
  std::vector<int64> variable_offset_;
  std::vector<int64> constraint_offset_;
  int first_constraint_to_offset_;
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
      int vehicle, const std::function<int64(int64)>& next_accessor,
      RoutingLinearSolverWrapper* solver, std::vector<int64>* cumul_values,
      std::vector<int64>* break_values, int64* cost, int64* transit_cost,
      bool clear_lp = true);

  bool Optimize(const std::function<int64(int64)>& next_accessor,
                RoutingLinearSolverWrapper* solver,
                std::vector<int64>* cumul_values,
                std::vector<int64>* break_values, int64* cost,
                int64* transit_cost, bool clear_lp = true);

  bool OptimizeAndPack(const std::function<int64(int64)>& next_accessor,
                       RoutingLinearSolverWrapper* solver,
                       std::vector<int64>* cumul_values,
                       std::vector<int64>* break_values);

  DimensionSchedulingStatus OptimizeAndPackSingleRoute(
      int vehicle, const std::function<int64(int64)>& next_accessor,
      RoutingLinearSolverWrapper* solver, std::vector<int64>* cumul_values,
      std::vector<int64>* break_values);

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
  bool ComputeRouteCumulBounds(const std::vector<int64>& route,
                               const std::vector<int64>& fixed_transits,
                               int64 cumul_offset);

  // Sets the constraints for all nodes on "vehicle"'s route according to
  // "next_accessor". If optimize_costs is true, also sets the objective
  // coefficients for the LP.
  // Returns false if some infeasibility was detected, true otherwise.
  bool SetRouteCumulConstraints(
      int vehicle, const std::function<int64(int64)>& next_accessor,
      int64 cumul_offset, bool optimize_costs,
      RoutingLinearSolverWrapper* solver, int64* route_transit_cost,
      int64* route_cost_offset);

  // Sets the global constraints on the dimension, and adds global objective
  // cost coefficients if optimize_costs is true.
  // NOTE: When called, the call to this function MUST come after
  // SetRouteCumulConstraints() has been called on all routes, so that
  // index_to_cumul_variable_ and min_start/max_end_cumul_ are correctly
  // initialized.
  void SetGlobalConstraints(bool optimize_costs,
                            RoutingLinearSolverWrapper* solver);

  void SetValuesFromLP(const std::vector<int>& lp_variables, int64 offset,
                       RoutingLinearSolverWrapper* solver,
                       std::vector<int64>* lp_values);

  // This function packs the routes of the given vehicles while keeping the cost
  // of the LP lower than its current (supposed optimal) objective value.
  // It does so by setting the current objective variables' coefficient to 0 and
  // setting the coefficient of the route ends to 1, to first minimize the route
  // ends' cumuls, and then maximizes the starts' cumuls without increasing the
  // ends.
  DimensionSchedulingStatus PackRoutes(std::vector<int> vehicles,
                                       RoutingLinearSolverWrapper* solver);

  std::unique_ptr<CumulBoundsPropagator> propagator_;
  std::vector<int64> current_route_min_cumuls_;
  std::vector<int64> current_route_max_cumuls_;
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

  int max_end_cumul_;
  int min_start_cumul_;
  std::vector<std::pair<int64, int64>>
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
      int vehicle, const std::function<int64(int64)>& next_accessor,
      int64* optimal_cost);

  // Same as ComputeRouteCumulCost, but the cost computed does not contain
  // the part of the vehicle span cost due to fixed transits.
  DimensionSchedulingStatus ComputeRouteCumulCostWithoutFixedTransits(
      int vehicle, const std::function<int64(int64)>& next_accessor,
      int64* optimal_cost_without_transits);

  // If feasible, computes the optimal values for cumul and break variables
  // of the route performed by a vehicle, minimizing cumul soft lower, upper
  // bound costs and vehicle span costs, stores them in "optimal_cumuls"
  // (if not null), and optimal_breaks, and returns true.
  // Returns false if the route is not feasible.
  DimensionSchedulingStatus ComputeRouteCumuls(
      int vehicle, const std::function<int64(int64)>& next_accessor,
      std::vector<int64>* optimal_cumuls, std::vector<int64>* optimal_breaks);

  // Similar to ComputeRouteCumuls, but also tries to pack the cumul values on
  // the route, such that the cost remains the same, the cumul of route end is
  // minimized, and then the cumul of the start of the route is maximized.
  DimensionSchedulingStatus ComputePackedRouteCumuls(
      int vehicle, const std::function<int64(int64)>& next_accessor,
      std::vector<int64>* packed_cumuls, std::vector<int64>* packed_breaks);

  const RoutingDimension* dimension() const {
    return optimizer_core_.dimension();
  }

 private:
  std::vector<std::unique_ptr<RoutingLinearSolverWrapper>> solver_;
  DimensionCumulOptimizerCore optimizer_core_;
};

class GlobalDimensionCumulOptimizer {
 public:
  explicit GlobalDimensionCumulOptimizer(const RoutingDimension* dimension);
  // If feasible, computes the optimal cost of the entire model with regards to
  // the optimizer_core_'s dimension costs, minimizing cumul soft lower/upper
  // bound costs and vehicle/global span costs, and stores it in "optimal_cost"
  // (if not null).
  // Returns true iff all the constraints can be respected.
  bool ComputeCumulCostWithoutFixedTransits(
      const std::function<int64(int64)>& next_accessor,
      int64* optimal_cost_without_transits);
  // If feasible, computes the optimal values for cumul and break variables,
  // minimizing cumul soft lower/upper bound costs and vehicle/global span
  // costs, stores them in "optimal_cumuls" (if not null) and optimal breaks,
  // and returns true.
  // Returns false if the routes are not feasible.
  bool ComputeCumuls(const std::function<int64(int64)>& next_accessor,
                     std::vector<int64>* optimal_cumuls,
                     std::vector<int64>* optimal_breaks);

  // Returns true iff the routes resulting from the next_accessor are feasible
  // wrt the constraints on the optimizer_core_.dimension()'s cumuls.
  bool IsFeasible(const std::function<int64(int64)>& next_accessor);

  // Similar to ComputeCumuls, but also tries to pack the cumul values on all
  // routes, such that the cost remains the same, the cumuls of route ends are
  // minimized, and then the cumuls of the starts of the routes are maximized.
  bool ComputePackedCumuls(const std::function<int64(int64)>& next_accessor,
                           std::vector<int64>* packed_cumuls,
                           std::vector<int64>* packed_breaks);

  const RoutingDimension* dimension() const {
    return optimizer_core_.dimension();
  }

 private:
  std::unique_ptr<RoutingLinearSolverWrapper> solver_;
  DimensionCumulOptimizerCore optimizer_core_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_LP_SCHEDULING_H_
