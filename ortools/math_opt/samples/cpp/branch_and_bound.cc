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

#include "ortools/math_opt/examples/cpp/branch_and_bound.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <queue>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research_example {
namespace {

namespace math_opt = operations_research::math_opt;
constexpr double kInf = std::numeric_limits<double>::infinity();

DEFINE_STRONG_INT_TYPE(VarIndex, int32_t);
DEFINE_STRONG_INT_TYPE(NodeId, int64_t);

////////////////////////////////////////////////////////////////////////////////
// Search Tree
////////////////////////////////////////////////////////////////////////////////

// A node in the search tree. Each node stores the branching decision leading to
// this node, the parent node, and the best known LP bound for this node. To
// recover all variable bounds, you must traverse back to the root node.
struct Node {
  std::optional<NodeId> parent;
  // In the unexplored state, this is the parent bound. In the explored state,
  // it is the lp bound.
  double bound = 0.0;
  VarIndex branch_variable = VarIndex{-1};
  int64_t variable_bound = 0;
  bool is_upper_bound = false;
  int num_children_live = 0;
  bool explored = false;
};

// Variable bounds to further restrict the LP relaxation at a node in the search
// tree.
struct Bounds {
  absl::flat_hash_map<VarIndex, int64_t> lower_bounds;
  absl::flat_hash_map<VarIndex, int64_t> upper_bounds;
};

// Stores the nodes of the search tree that are either not yet explored, or
// have a child that is not yet explored.
//
// The unexplored nodes are stored in "frontier_", which, for minimization,
// stores them in the order of lowest LP bound first. You can access this node
// and its id by `top()` and `top_id()`.
//
// To process a node, you first must get all the variable bounds for this node
// (with `RecoverBounds()`) and then solve the LP relaxation with these bounds.
//
// After processing the top node, you can:
//   * Close it (by `CloseTop()`), which deletes it and potentially its parents.
//     Take this action if the solution was integer, or if the LP bound was
//     larger (for minimization) than the best integer solution found.
//   * Branch into two new nodes. Take this action for a fractional solution
//     with LP bound less than (for minimization) the best integer solution.
//
// A global bound on your problem is (for minimization) the minimum of the
// objective value found for any integer solution, and the lowest bound on any
// open node from the frontier. This is a valid bound because we only close a
// node when either it is integer or when the bound is larger than the best
// integer solution we have found. If we have not found an integer solution and
// there are no nodes remaining, then we have proven the problem infeasible (the
// bound is +inf). Note that that open nodes typically have bound lower than the
// best integer solution found (as otherwise we immediately close them). We can
// efficiently the compute the bound over all open nodes by looking at `top()`
// because we store the nodes in a priority queue with the order of lowest bound
// first.
class SearchTree {
 public:
  explicit SearchTree(bool is_maximize);

  // Indicates there are no nodes left to process.
  bool frontier_empty() const { return frontier_.empty(); }

  // The id of the next node to process.
  //
  // Behavior is undefined when `frontier_empty()` is true.
  NodeId top_id() const { return frontier_.top().second; }

  // A mutable reference to the next node to process.
  //
  // Behavior is undefined when `frontier_empty()` is true.
  Node& top() { return nodes_.at(top_id()); }

  // A const reference to the next node to process.
  //
  // Behavior is undefined when `frontier_empty()` is true.
  const Node& top() const { return nodes_.at(top_id()); }

  // Marks the top node as explored and adds two child nodes to the frontier.
  //
  // Behavior is undefined when `frontier_empty()` is true.
  void BranchOnTop(VarIndex branching_var, int64_t branch_down_value);

  // Deletes the top node and then recursively deletes ancestors that have no
  // open children.
  //
  // Behavior is undefined when `frontier_empty()` is true.
  void CloseTop();

  // Traverses the tree back to the root node to get variable bounds for node.
  Bounds RecoverBounds(NodeId node) const;

  // Returns the global bound on the objective value of this problem. Is nullopt
  // when `frontier_empty()`, see class level comment.
  std::optional<double> bound() const;

 private:
  void frontier_push(double bound, NodeId node);
  NodeId add_node(const Node& node);

  // If the optimization problem is has a maximization objective.
  bool is_maximize_;

  // The nodes that are unexplored or that have unexplored children.
  absl::flat_hash_map<NodeId, Node> nodes_;

  // The nodes that are unexplored, ordered by:
  //   * For minimization, lowest LP relaxation first,
  //   * For maximization, highest LP relaxation first.
  std::priority_queue<std::pair<double, NodeId>> frontier_;

  // The id to use for the next node created (ids are not reused).
  NodeId next_id_ = NodeId{0};
};

void SearchTree::frontier_push(double bound, const NodeId node) {
  // frontier returns the largest elements first, which is correct for
  // maximization problems, but the opposite of what we want for minimization.
  if (!is_maximize_) {
    bound = -bound;
  }
  frontier_.push({bound, node});
}

NodeId SearchTree::add_node(const Node& node) {
  const NodeId id = next_id_;
  nodes_[id] = node;
  frontier_push(node.bound, id);
  ++next_id_;
  return id;
}

SearchTree::SearchTree(const bool is_maximize) : is_maximize_(is_maximize) {
  double bound = is_maximize ? kInf : -kInf;
  add_node({.bound = bound});
}

Bounds SearchTree::RecoverBounds(const NodeId node_id) const {
  Bounds result;
  std::optional<NodeId> next = node_id;
  while (next.has_value()) {
    const Node& node = nodes_.at(*next);
    if (node.branch_variable >= VarIndex{0}) {
      if (node.is_upper_bound) {
        // If the key is already present, the key from lower in the tree is
        // tighter, so discard this value.
        result.upper_bounds.insert({node.branch_variable, node.variable_bound});
      } else {
        result.lower_bounds.insert({node.branch_variable, node.variable_bound});
      }
    }
    next = node.parent;
  }
  return result;
}

void SearchTree::BranchOnTop(const VarIndex branching_var,
                             const int64_t branch_down_value) {
  const auto [unused, top] = frontier_.top();
  frontier_.pop();
  Node& top_node = nodes_.at(top);
  const double new_bound = top_node.bound;
  top_node.num_children_live = 2;
  top_node.explored = true;
  const Node down = {.parent = top,
                     .bound = new_bound,
                     .branch_variable = branching_var,
                     .variable_bound = branch_down_value,
                     .is_upper_bound = true};
  const Node up = {.parent = top,
                   .bound = new_bound,
                   .branch_variable = branching_var,
                   .variable_bound = branch_down_value + 1,
                   .is_upper_bound = false};
  add_node(down);
  add_node(up);
}

void SearchTree::CloseTop() {
  std::optional<NodeId> to_delete = frontier_.top().second;
  frontier_.pop();
  while (to_delete.has_value()) {
    const auto node_it = nodes_.find(*to_delete);
    CHECK(node_it != nodes_.end());
    Node& node = node_it->second;
    if (node.explored) {
      --node.num_children_live;
      if (node.num_children_live > 0) {
        break;
      }
    }
    to_delete = node.parent;
    nodes_.erase(node_it);
  }
}

std::optional<double> SearchTree::bound() const {
  if (frontier_.empty()) {
    return std::nullopt;
  }
  return top().bound;
}

////////////////////////////////////////////////////////////////////////////////
// LP Relaxation
////////////////////////////////////////////////////////////////////////////////

// All fields other than termination are filled only when termination reason is
// optimal.
struct LpSolution {
  math_opt::Termination termination;
  // TODO(b/290091715): delete these once they are part of termination
  double objective_value = 0.0;
  double dual_bound = 0.0;
  absl::StrongVector<VarIndex, double> variable_values;
  std::vector<VarIndex> integer_vars_with_fractional_values;
  int64_t pivots = 0;

  bool is_integer() const {
    return integer_vars_with_fractional_values.empty();
  }
};

// Solves the linear programming (LP) relaxation of an input optimization model.
//
// Copies the input model and to build a modified model, and builds a solver on
// the relaxed model.
class LpRelaxation {
 public:
  // Holds a reference to `model`. The caller MUST ensure that `model` outlives
  // this.
  static absl::StatusOr<std::unique_ptr<LpRelaxation>> New(
      const math_opt::Model& model, math_opt::SolverType solver,
      double integrality_abs_tolerance);

  // Modifies the variable bounds of the LP relaxation to `bounds`. Typically
  // call `RestoreBounds()` first.
  void SetBounds(const Bounds& bounds);

  // Sets the variable bounds of the LP relaxation back to their bounds in the
  // input model.
  void RestoreBounds();

  // Solves the LP relaxation and returns the result.
  absl::StatusOr<LpSolution> Solve(
      const math_opt::SolveParameters& solve_params);

  // Given a solution to the LP relaxation, rewrite it on the Variable objects
  // of the input model.
  math_opt::VariableMap<double> RestoreMipSolution(
      const absl::StrongVector<VarIndex, double>& lp_solution) const;

 private:
  // Maintains a 1:1 mapping between variables of the LP relaxation and the
  // input model.
  struct VarData {
    // From the LP relaxation.
    math_opt::Variable variable;
    // From the input model.
    math_opt::Variable orig_variable;

    bool was_integer() const { return orig_variable.is_integer(); }

    double init_lb() const { return orig_variable.lower_bound(); }
    double init_ub() const { return orig_variable.upper_bound(); }
  };

  LpRelaxation() = default;

  std::unique_ptr<math_opt::Model> relaxed_model_;
  std::unique_ptr<math_opt::IncrementalSolver> solver_;
  double integrality_abs_tolerance_ = 0.0;
  absl::StrongVector<VarIndex, VarData> var_data_;
};

absl::StatusOr<std::unique_ptr<LpRelaxation>> LpRelaxation::New(
    const math_opt::Model& model, const math_opt::SolverType solver,
    const double integrality_abs_tolerance) {
  const std::vector<math_opt::Variable> orig_variables =
      model.SortedVariables();
  auto result = absl::WrapUnique(new LpRelaxation());
  result->relaxed_model_ = model.Clone();
  const std::vector<math_opt::Variable> new_variables =
      result->relaxed_model_->SortedVariables();

  for (int i = 0; i < orig_variables.size(); ++i) {
    result->relaxed_model_->set_continuous(new_variables[i]);
    result->var_data_.push_back(
        {.variable = new_variables[i], .orig_variable = orig_variables[i]});
  }
  result->integrality_abs_tolerance_ = integrality_abs_tolerance;
  ASSIGN_OR_RETURN(result->solver_, math_opt::IncrementalSolver::New(
                                        result->relaxed_model_.get(), solver));
  return result;
}

void LpRelaxation::SetBounds(const Bounds& bounds) {
  for (const auto [var_index, value] : bounds.lower_bounds) {
    relaxed_model_->set_lower_bound(var_data_[var_index].variable, value);
  }
  for (const auto [var_index, value] : bounds.upper_bounds) {
    relaxed_model_->set_upper_bound(var_data_[var_index].variable, value);
  }
}

void LpRelaxation::RestoreBounds() {
  for (const VarData& var_data : var_data_) {
    relaxed_model_->set_lower_bound(var_data.variable, var_data.init_lb());
    relaxed_model_->set_upper_bound(var_data.variable, var_data.init_ub());
  }
}

absl::StatusOr<LpSolution> LpRelaxation::Solve(
    const math_opt::SolveParameters& params) {
  ASSIGN_OR_RETURN(const math_opt::SolveResult lp_result,
                   solver_->Solve({.parameters = params}));
  LpSolution solution = {.termination = lp_result.termination,
                         .pivots = lp_result.solve_stats.simplex_iterations};
  if (lp_result.termination.reason == math_opt::TerminationReason::kOptimal) {
    solution.objective_value = lp_result.objective_value();
    solution.dual_bound = lp_result.best_objective_bound();
    for (const VarIndex v : var_data_.index_range()) {
      const VarData& var_data = var_data_[v];
      const double var_value =
          lp_result.variable_values().at(var_data.variable);
      solution.variable_values.push_back(var_value);
      if (var_data.was_integer()) {
        double err = std::abs(std::round(var_value) - var_value);
        if (err > integrality_abs_tolerance_) {
          solution.integer_vars_with_fractional_values.push_back(v);
        }
      }
    }
  }
  return solution;
}

math_opt::VariableMap<double> LpRelaxation::RestoreMipSolution(
    const absl::StrongVector<VarIndex, double>& lp_solution) const {
  math_opt::VariableMap<double> result;
  for (const VarIndex v : var_data_.index_range()) {
    result[var_data_[v].orig_variable] = lp_solution[v];
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// Solve State and Stats (measure progress)
////////////////////////////////////////////////////////////////////////////////

// Tracks the progress of the solver and if we have reached a termination
// criteria.
class SolveState {
 public:
  SolveState(BranchAndBoundParameters parameters, bool is_maximize);

  bool should_terminate() const {
    return absl::Now() >= deadline_ || is_within_gap();
  }

  bool is_within_gap() const {
    if (!std::isfinite(best_primal_bound_)) {
      return false;
    }
    double absolute_gap = best_primal_bound_ - best_dual_bound_;
    if (is_maximize_) {
      absolute_gap = -absolute_gap;
    }
    return absolute_gap <=
           parameters_.abs_gap_tolerance +
               parameters_.rel_gap_tolerance * std::abs(best_primal_bound_);
  }

  // Providing a value of std::nullopt indicates that the search tree is empty.
  // In this case, the problem either optimal if we have found an integer,
  // or infeasible if we have not. In both cases, the dual bound is now equal
  // to the primal bound.
  void update_dual_bound(std::optional<double> bound);

  void update_primal_bound(absl::StrongVector<VarIndex, double> solution,
                           double objective_value);

  absl::Duration time_remaining() const { return deadline_ - absl::Now(); }
  absl::Duration elapsed_time() const { return absl::Now() - start_; }

  SimpleSolveResult Result(const LpRelaxation& relaxation,
                           bool search_tree_empty,
                           const SimpleSolveResult::Stats& stats) const;

  bool is_better_than_best_solution(double new_obj) {
    if (!best_integer_solution_.has_value()) {
      return true;
    }
    if (is_maximize_) {
      return new_obj > best_primal_bound_;
    } else {
      return new_obj < best_primal_bound_;
    }
  }

  double best_primal_bound() const { return best_primal_bound_; }
  double best_dual_bound() const { return best_dual_bound_; }
  absl::Time start_time() const { return start_; }

 private:
  BranchAndBoundParameters parameters_;
  bool is_maximize_;
  absl::Time start_;
  absl::Time deadline_;
  std::optional<absl::StrongVector<VarIndex, double>> best_integer_solution_;
  double best_primal_bound_;
  double best_dual_bound_;
};

SimpleSolveResult SolveState::Result(
    const LpRelaxation& relaxation, const bool search_tree_empty,
    const SimpleSolveResult::Stats& stats) const {
  SimpleSolveResult solve_result{.primal_bound = best_primal_bound_,
                                 .dual_bound = best_dual_bound_,
                                 .stats = stats};
  if (!best_integer_solution_.has_value()) {
    if (search_tree_empty) {
      return InfeasibleSolveResult(is_maximize_, stats);
    }
    solve_result.termination_reason = SimpleSolveResult::Reason::kNoSolution;
    return solve_result;
  }
  solve_result.variable_values =
      relaxation.RestoreMipSolution(*best_integer_solution_);
  if (is_within_gap()) {
    solve_result.termination_reason = SimpleSolveResult::Reason::kOptimal;
  } else {
    solve_result.termination_reason = SimpleSolveResult::Reason::kFeasible;
  }
  return solve_result;
}

SolveState::SolveState(const BranchAndBoundParameters parameters,
                       const bool is_maximize)
    : parameters_(std::move(parameters)), is_maximize_(is_maximize) {
  start_ = absl::Now();
  deadline_ = start_ + parameters_.time_limit;
  if (is_maximize_) {
    best_primal_bound_ = -kInf;
    best_dual_bound_ = kInf;
  } else {
    best_primal_bound_ = kInf;
    best_dual_bound_ = -kInf;
  }
}

void SolveState::update_dual_bound(std::optional<double> bound) {
  if (bound.has_value()) {
    best_dual_bound_ = *bound;
  } else {
    // This is subtle, see documentation on update_dual_bound declaration.
    best_dual_bound_ = best_primal_bound_;
  }
}

void SolveState::update_primal_bound(
    absl::StrongVector<VarIndex, double> solution, double objective_value) {
  if (is_better_than_best_solution(objective_value)) {
    best_primal_bound_ = objective_value;
    best_integer_solution_ = std::move(solution);
  }
}

void PrintSearchHeader(const BranchAndBoundParameters& params) {
  if (params.enable_output) {
    std::cout << absl::StrFormat("%13s | %8s | %8s | %13s | %13s | %10s",
                                 "time", "nodes", "closed", "objective",
                                 "bound", "pivot/node")
              << std::endl;
  }
}

void PrintSearchRow(const BranchAndBoundParameters& params,
                    const SimpleSolveResult::Stats& stats,
                    const SolveState& solve_state) {
  const int64_t n = stats.nodes_closed;
  // Print a log line for the first 10 nodes solved, and then only when the
  // number of nodes solved is a power of two.
  if (params.enable_output && (n <= 10 || (n & (n - 1)) == 0)) {
    const double pivots_per_closed_node =
        stats.nodes_closed == 0
            ? 0.0
            : static_cast<double>(stats.tree_pivots) / stats.nodes_closed;
    std::cout << absl::StrFormat(
                     "%13s | %8d | %8d | %13.4f | %13.4f | %10.2f",
                     absl::FormatDuration(solve_state.elapsed_time()),
                     stats.nodes_created, stats.nodes_closed,
                     solve_state.best_primal_bound(),
                     solve_state.best_dual_bound(), pivots_per_closed_node)
              << std::endl;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Branch and Bound Algorithm
////////////////////////////////////////////////////////////////////////////////

// NOTE: this is a very simple but not very good branching rule, typically
// prefer strong branching or pseudo-costs. Better branching rules are stateful
// and/or need access to the LP relaxation to do extra solves.
absl::StatusOr<VarIndex> MostFractionalBranchingRule(
    const LpSolution& lp_solution) {
  std::optional<VarIndex> most_fractional;
  double most_fractional_size = 0.0;
  for (const VarIndex v : lp_solution.integer_vars_with_fractional_values) {
    const double v_val = lp_solution.variable_values[v];
    const double fractional_value = std::abs(std::round(v_val) - v_val);
    if (fractional_value > most_fractional_size) {
      most_fractional_size = fractional_value;
      most_fractional = v;
    }
  }
  if (!most_fractional.has_value()) {
    return absl::InternalError(
        "failed to find a fractional variable for branching, should be "
        "impossible");
  }
  return *most_fractional;
}

absl::StatusOr<SimpleSolveResult> SolveWithBranchAndBoundImpl(
    const math_opt::Model& model, const BranchAndBoundParameters& params) {
  const bool is_maximize = model.is_maximize();
  SimpleSolveResult::Stats stats;

  SolveState solve_state(params, is_maximize);
  if (params.enable_output) {
    std::cout << "Solving LP Relaxation: " << std::endl;
  }
  ASSIGN_OR_RETURN(auto lp_solver,
                   LpRelaxation::New(model, params.lp_solver,
                                     params.integrality_absolute_tolerance));

  // Solve the root separately, a few extra special cases to take care of:
  //  * The problem can actually be unbounded (infeasible or unbounded does not
  //    necessarily imply infeasible).
  //  * We need to ensure that we use dual simplex in the tree, but solver can
  //    decide the method used in the root.
  //  * Future versions may want to save the basis or add cuts at the root.
  math_opt::SolveParameters root_params = {
      .enable_output = params.enable_output,
      .time_limit =
          std::max(absl::ZeroDuration(), solve_state.time_remaining())};
  // We do not get effective incremental solves with GLOP when presolve is on.
  if (params.lp_solver == math_opt::SolverType::kGlop) {
    root_params.presolve = math_opt::Emphasis::kOff;
  }
  ASSIGN_OR_RETURN(LpSolution root_solution, lp_solver->Solve(root_params));
  stats.root_pivots = root_solution.pivots;
  if (params.enable_output) {
    std::cout << "LP Relaxation termination: " << root_solution.termination
              << " pivots: " << root_solution.pivots << std::endl;
  }
  switch (root_solution.termination.reason) {
    case math_opt::TerminationReason::kImprecise:
    case math_opt::TerminationReason::kNumericalError:
    case math_opt::TerminationReason::kOtherError:
      return ErrorSolveResult(is_maximize, stats);
    case math_opt::TerminationReason::kInfeasible:
      return InfeasibleSolveResult(is_maximize, stats);
    case math_opt::TerminationReason::kInfeasibleOrUnbounded:
    case math_opt::TerminationReason::kUnbounded:
      // When the LP is unbounded, we do not yet have an integer feasible point,
      // so the problem may be infeasible. You need to solve with zero
      // objective and find an integer feasible point to conclude unbounded.
      return InfeasibleOrUnboundedSolveResult(is_maximize, stats);
    case math_opt::TerminationReason::kNoSolutionFound:
    case math_opt::TerminationReason::kFeasible:
      solve_state.update_dual_bound(root_solution.dual_bound);
      return solve_state.Result(*lp_solver, false, stats);
    case math_opt::TerminationReason::kOptimal:
      solve_state.update_dual_bound(root_solution.dual_bound);
      if (root_solution.is_integer()) {
        solve_state.update_primal_bound(root_solution.variable_values,
                                        root_solution.objective_value);
      }
      if (solve_state.is_within_gap()) {
        return solve_state.Result(*lp_solver, false, stats);
      }
      break;
  }
  // Invariant: we have solved the LP relaxation to optimality (and thus the
  // problem is bounded, although could still be infeasible).
  SearchTree tree(is_maximize);
  ++stats.nodes_created;
  // NOTE: we solve the root LP twice, but because the solve is incremental,
  // the second solve is essentially free.
  PrintSearchHeader(params);
  while (!tree.frontier_empty() && !solve_state.should_terminate()) {
    PrintSearchRow(params, stats, solve_state);
    NodeId top_id = tree.top_id();
    Node& top = tree.top();
    lp_solver->RestoreBounds();
    const Bounds bounds = tree.RecoverBounds(top_id);
    lp_solver->SetBounds(bounds);
    math_opt::SolveParameters tree_params = {
        .time_limit =
            std::max(absl::ZeroDuration(), solve_state.time_remaining())};
    // We do not get effective incremental solves with GLOP when presolve is on.
    // We want dual simplex, since our old solution is dual feasible, but GLOP
    // does not automatically select it with the default settings.
    if (params.lp_solver == math_opt::SolverType::kGlop) {
      tree_params.presolve = math_opt::Emphasis::kOff;
      tree_params.lp_algorithm = math_opt::LPAlgorithm::kDualSimplex;
    }
    ASSIGN_OR_RETURN(LpSolution lp_solution, lp_solver->Solve(tree_params));
    stats.tree_pivots += lp_solution.pivots;
    ++stats.nodes_closed;
    switch (lp_solution.termination.reason) {
      case math_opt::TerminationReason::kImprecise:
      case math_opt::TerminationReason::kNumericalError:
      case math_opt::TerminationReason::kOtherError:
      case math_opt::TerminationReason::kUnbounded:
        // Unbounded is now an error, this should have been caught at the root.
        return ErrorSolveResult(is_maximize, stats);
      case math_opt::TerminationReason::kNoSolutionFound:
      case math_opt::TerminationReason::kFeasible:
        // We are out of time, terminate.
        // Warning: if more termination criteria are added (e.g. the use of a
        // cutoff when solving the LP relaxation, as is typical in branch and
        // bound), then you need to check `lp_solution.termination.limit` to
        // decide what to do here.
        return solve_state.Result(*lp_solver, false, stats);
      case math_opt::TerminationReason::kInfeasible:
      case math_opt::TerminationReason::kInfeasibleOrUnbounded:
        // Infeasible or unbounded must be infeasible, as we have already
        // ruled out unbounded.
        tree.CloseTop();
        break;
      case math_opt::TerminationReason::kOptimal: {
        top.bound = lp_solution.objective_value;
        if (lp_solution.is_integer()) {
          solve_state.update_primal_bound(lp_solution.variable_values,
                                          lp_solution.objective_value);
          tree.CloseTop();
        } else if (solve_state.is_better_than_best_solution(top.bound)) {
          ASSIGN_OR_RETURN(const VarIndex branch_var,
                           MostFractionalBranchingRule(lp_solution));
          const int64_t branch_down_val = static_cast<int64_t>(
              std::floor(lp_solution.variable_values[branch_var]));
          tree.BranchOnTop(branch_var, branch_down_val);
          stats.nodes_created += 2;
        } else {
          tree.CloseTop();
        }
        break;
      }
    }
    solve_state.update_dual_bound(tree.bound());
  }
  PrintSearchRow(params, stats, solve_state);
  return solve_state.Result(*lp_solver, tree.frontier_empty(), stats);
}

}  // namespace

SimpleSolveResult TrivialSolveResult(bool is_maximize,
                                     const SimpleSolveResult::Stats& stats) {
  SimpleSolveResult result{
      .termination_reason = SimpleSolveResult::Reason::kNoSolution,
      .stats = stats};
  if (is_maximize) {
    result.primal_bound = -kInf;
    result.dual_bound = kInf;
  } else {
    result.primal_bound = kInf;
    result.dual_bound = -kInf;
  }
  return result;
}

SimpleSolveResult ErrorSolveResult(bool is_maximize,
                                   const SimpleSolveResult::Stats& stats) {
  SimpleSolveResult result = TrivialSolveResult(is_maximize, stats);
  result.termination_reason = SimpleSolveResult::Reason::kError;
  return result;
}

SimpleSolveResult InfeasibleSolveResult(bool is_maximize,
                                        const SimpleSolveResult::Stats& stats) {
  SimpleSolveResult result = TrivialSolveResult(is_maximize, stats);
  result.termination_reason = SimpleSolveResult::Reason::kInfeasible;
  return result;
}
SimpleSolveResult InfeasibleOrUnboundedSolveResult(
    bool is_maximize, const SimpleSolveResult::Stats& stats) {
  SimpleSolveResult result = TrivialSolveResult(is_maximize, stats);
  result.termination_reason = SimpleSolveResult::Reason::kInfeasibleOrUnbounded;
  return result;
}

std::ostream& operator<<(std::ostream& ostr,
                         const SimpleSolveResult::Reason reason) {
  switch (reason) {
    case SimpleSolveResult::Reason::kOptimal:
      ostr << "optimal";
      return ostr;
    case SimpleSolveResult::Reason::kFeasible:
      ostr << "feasible";
      return ostr;
    case SimpleSolveResult::Reason::kNoSolution:
      ostr << "no_solution";
      return ostr;
    case SimpleSolveResult::Reason::kInfeasible:
      ostr << "infeasible";
      return ostr;
    case SimpleSolveResult::Reason::kInfeasibleOrUnbounded:
      ostr << "infeasible_or_unbounded";
      return ostr;
    case SimpleSolveResult::Reason::kError:
      ostr << "error";
      return ostr;
  }
}

std::ostream& operator<<(std::ostream& ostr,
                         const SimpleSolveResult::Stats& stats) {
  ostr << "solve_time: " << stats.solve_time
       << " root_pivots: " << stats.root_pivots
       << " tree_pivots: " << stats.tree_pivots
       << " nodes created: " << stats.nodes_created
       << " nodes closed: " << stats.nodes_closed;
  return ostr;
}

absl::StatusOr<SimpleSolveResult> SolveWithBranchAndBound(
    const math_opt::Model& model, const BranchAndBoundParameters& params) {
  const absl::Time start = absl::Now();
  ASSIGN_OR_RETURN(SimpleSolveResult result,
                   SolveWithBranchAndBoundImpl(model, params));
  result.stats.solve_time = absl::Now() - start;
  if (params.enable_output) {
    std::cout << "Branch and bound terminated." << std::endl;
    std::cout << "termination reason: " << result.termination_reason
              << std::endl;
    std::cout << "primal bound: " << result.primal_bound << std::endl;
    std::cout << "dual bound: " << result.dual_bound << std::endl;
    std::cout << "final stats:\n" << result.stats << std::endl;
  }
  return result;
}

}  // namespace operations_research_example
