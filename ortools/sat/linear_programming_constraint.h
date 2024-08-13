// Copyright 2010-2024 Google LLC
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

#ifndef OR_TOOLS_SAT_LINEAR_PROGRAMMING_CONSTRAINT_H_
#define OR_TOOLS_SAT_LINEAR_PROGRAMMING_CONSTRAINT_H_

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/numeric/int128.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/revised_simplex.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_data_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/sat/zero_half_cuts.h"
#include "ortools/util/rev.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Simple class to combine linear expression efficiently. First in a sparse
// way that switch to dense when the number of non-zeros grows.
class ScatteredIntegerVector {
 public:
  // This must be called with the correct size before any other functions are
  // used.
  void ClearAndResize(int size);

  // Does vector[col] += value and return false in case of overflow.
  bool Add(glop::ColIndex col, IntegerValue value);

  // Similar to Add() but for multiplier * terms.
  //
  // Returns false if we encountered any integer overflow. If the template bool
  // is false, we do not check for a bit of extra speed.
  template <bool check_overflow = true>
  bool AddLinearExpressionMultiple(IntegerValue multiplier,
                                   absl::Span<const glop::ColIndex> cols,
                                   absl::Span<const IntegerValue> coeffs);

  // This is not const only because non_zeros is sorted. Note that sorting the
  // non-zeros make the result deterministic whether or not we were in sparse
  // mode.
  //
  // TODO(user): Ideally we should convert to IntegerVariable as late as
  // possible. Prefer to use GetTerms().
  LinearConstraint ConvertToLinearConstraint(
      absl::Span<const IntegerVariable> integer_variables,
      IntegerValue upper_bound,
      std::optional<std::pair<IntegerVariable, IntegerValue>> extra_term =
          std::nullopt);

  void ConvertToCutData(absl::int128 rhs,
                        absl::Span<const IntegerVariable> integer_variables,
                        absl::Span<const double> lp_solution,
                        IntegerTrail* integer_trail, CutData* result);

  // Similar to ConvertToLinearConstraint().
  std::vector<std::pair<glop::ColIndex, IntegerValue>> GetTerms();

  // We only provide the const [].
  IntegerValue operator[](glop::ColIndex col) const {
    return dense_vector_[col];
  }

  bool IsSparse() const { return is_sparse_; }

 private:
  // If is_sparse is true we maintain the non_zeros positions and bool vector
  // of dense_vector_. Otherwise we don't. Note that we automatically switch
  // from sparse to dense as needed.
  bool is_sparse_ = true;
  std::vector<glop::ColIndex> non_zeros_;
  util_intops::StrongVector<glop::ColIndex, bool> is_zeros_;

  // The dense representation of the vector.
  util_intops::StrongVector<glop::ColIndex, IntegerValue> dense_vector_;
};

// A SAT constraint that enforces a set of linear inequality constraints on
// integer variables using an LP solver.
//
// The propagator uses glop's revised simplex for feasibility and propagation.
// It uses the Reduced Cost Strengthening technique, a classic in mixed integer
// programming, for instance see the thesis of Tobias Achterberg,
// "Constraint Integer Programming", sections 7.7 and 8.8, algorithm 7.11.
// http://nbn-resolving.de/urn:nbn:de:0297-zib-11129
//
// Per-constraint bounds propagation is NOT done by this constraint,
// it should be done by redundant constraints, as reduced cost propagation
// may miss some filtering.
//
// Note that this constraint works with double floating-point numbers, so one
// could be worried that it may filter too much in case of precision issues.
// However, by default, we interpret the LP result by recomputing everything
// in integer arithmetic, so we are exact.
class LinearProgrammingDispatcher;

class LinearProgrammingConstraint : public PropagatorInterface,
                                    ReversibleInterface {
 public:
  typedef glop::RowIndex ConstraintIndex;

  // Each linear programming constraint works on a fixed set of variables.
  // We expect the set of variable to be sorted in increasing order.
  LinearProgrammingConstraint(Model* model,
                              absl::Span<const IntegerVariable> vars);

  // Add a new linear constraint to this LP.
  void AddLinearConstraint(LinearConstraint ct);

  // Set the coefficient of the variable in the objective. Calling it twice will
  // overwrite the previous value.
  void SetObjectiveCoefficient(IntegerVariable ivar, IntegerValue coeff);

  // The main objective variable should be equal to the linear sum of
  // the arguments passed to SetObjectiveCoefficient().
  void SetMainObjectiveVariable(IntegerVariable ivar) { objective_cp_ = ivar; }
  IntegerVariable ObjectiveVariable() const { return objective_cp_; }

  // Register a new cut generator with this constraint.
  void AddCutGenerator(CutGenerator generator);

  // Returns the LP value and reduced cost of a variable in the current
  // solution. These functions should only be called when HasSolution() is true.
  //
  // Note that this solution is always an OPTIMAL solution of an LP above or
  // at the current decision level. We "erase" it when we backtrack over it.
  bool HasSolution() const { return lp_solution_is_set_; }
  double GetSolutionValue(IntegerVariable variable) const;
  double GetSolutionReducedCost(IntegerVariable variable) const;
  bool SolutionIsInteger() const { return lp_solution_is_integer_; }

  // Returns a valid lp lower bound for the current branch, and indicates if
  // the latest LP solve in that branch was solved to optimality or not.
  // During normal operation, we will always propagate the LP, so this should
  // not refer to an optimality status lower in that branch.
  bool AtOptimal() const { return lp_at_optimal_; }
  double ObjectiveLpLowerBound() const { return lp_objective_lower_bound_; }

  // PropagatorInterface API.
  bool Propagate() override;
  bool IncrementalPropagate(const std::vector<int>& watch_indices) override;
  void RegisterWith(Model* model);

  // ReversibleInterface API.
  void SetLevel(int level) override;

  int NumVariables() const {
    return static_cast<int>(integer_variables_.size());
  }
  const std::vector<IntegerVariable>& integer_variables() const {
    return integer_variables_;
  }
  std::string DimensionString() const { return lp_data_.GetDimensionString(); }

  // Returns a IntegerLiteral guided by the underlying LP constraints.
  //
  // This computes the mean of reduced costs over successive calls,
  // and tries to fix the variable which has the highest reduced cost.
  // Tie-breaking is done using the variable natural order.
  std::function<IntegerLiteral()> HeuristicLpReducedCostAverageBranching();

  // Average number of nonbasic variables with zero reduced costs.
  double average_degeneracy() const {
    return average_degeneracy_.CurrentAverage();
  }

  // Stats.
  int64_t total_num_simplex_iterations() const {
    return total_num_simplex_iterations_;
  }
  int64_t total_num_cut_propagations() const {
    return total_num_cut_propagations_;
  }
  int64_t total_num_eq_propagations() const {
    return total_num_eq_propagations_;
  }
  int64_t num_solves() const { return num_solves_; }
  int64_t num_adjusts() const { return num_adjusts_; }
  int64_t num_cut_overflows() const { return num_cut_overflows_; }
  int64_t num_bad_cuts() const { return num_bad_cuts_; }
  int64_t num_scaling_issues() const { return num_scaling_issues_; }

  // This can serve as a timestamp to know if a saved basis is out of date.
  int64_t num_lp_changes() const { return num_lp_changes_; }

  const std::vector<int64_t>& num_solves_by_status() const {
    return num_solves_by_status_;
  }

  const LinearConstraintManager& constraint_manager() const {
    return constraint_manager_;
  }

  // Important: this is only temporarily valid.
  IntegerSumLE128* LatestOptimalConstraintOrNull() const {
    if (optimal_constraints_.empty()) return nullptr;
    return optimal_constraints_.back().get();
  }

  const std::vector<std::unique_ptr<IntegerSumLE128>>& OptimalConstraints()
      const {
    return optimal_constraints_;
  }

  // This api allows to temporarily disable the LP propagator which can be
  // costly during probing or other heavy propagation phase.
  void EnablePropagation(bool enable) {
    enabled_ = enable;
    watcher_->CallOnNextPropagate(watcher_id_);
  }
  bool PropagationIsEnabled() const { return enabled_; }

  const glop::BasisState& GetBasisState() const { return state_; }
  void LoadBasisState(const glop::BasisState& state) {
    state_ = state;
    simplex_.LoadStateForNextSolve(state_);
  }

 private:
  // Helper method to fill reduced cost / dual ray reason in 'integer_reason'.
  // Generates a set of IntegerLiterals explaining why the best solution can not
  // be improved using reduced costs. This is used to generate explanations for
  // both infeasibility and bounds deductions.
  void FillReducedCostReasonIn(const glop::DenseRow& reduced_costs,
                               std::vector<IntegerLiteral>* integer_reason);

  // Reinitialize the LP from a potentially new set of constraints.
  // This fills all data structure and properly rescale the underlying LP.
  //
  // Returns false if the problem is UNSAT (it can happen when presolve is off
  // and some LP constraint are trivially false).
  bool CreateLpFromConstraintManager();

  // Solve the LP, returns false if something went wrong in the LP solver.
  bool SolveLp();

  // Analyzes the result of an LP Solution. Returns false on conflict.
  bool AnalyzeLp();

  // Does some basic preprocessing of a cut candidate. Returns false if we
  // should abort processing this candidate.
  bool PreprocessCut(IntegerVariable first_slack, CutData* cut);

  // Add a "MIR" cut obtained by first taking the linear combination of the
  // row of the matrix according to "integer_multipliers" and then trying
  // some integer rounding heuristic.
  //
  // Return true if a new cut was added to the cut manager.
  bool AddCutFromConstraints(
      absl::string_view name,
      absl::Span<const std::pair<glop::RowIndex, IntegerValue>>
          integer_multipliers);

  // Second half of AddCutFromConstraints().
  bool PostprocessAndAddCut(const std::string& name, const std::string& info,
                            IntegerVariable first_slack, const CutData& cut);

  // Computes and adds the corresponding type of cuts.
  // This can currently only be called at the root node.
  void AddObjectiveCut();
  void AddCGCuts();
  void AddMirCuts();
  void AddZeroHalfCuts();

  // Updates the bounds of the LP variables from the CP bounds.
  void UpdateBoundsOfLpVariables();

  // Use the dual optimal lp values to compute an EXACT lower bound on the
  // objective. Fills its reason and perform reduced cost strenghtening.
  // Returns false in case of conflict.
  bool PropagateExactLpReason();

  // Same as FillDualRayReason() but perform the computation EXACTLY. Returns
  // false in the case that the problem is not provably infeasible with exact
  // computations, true otherwise.
  bool PropagateExactDualRay();

  // Called by PropagateExactLpReason() and PropagateExactDualRay() to finish
  // propagation.
  bool PropagateLpConstraint(LinearConstraint ct);

  // Returns number of non basic variables with zero reduced costs.
  int64_t CalculateDegeneracy();

  // From a set of row multipliers (at LP scale), scale them back to the CP
  // world and then make them integer (eventually multiplying them by a new
  // scaling factor returned in *scaling).
  //
  // Note that this will loose some precision, but our subsequent computation
  // will still be exact as it will work for any set of multiplier.
  std::vector<std::pair<glop::RowIndex, IntegerValue>> ScaleLpMultiplier(
      bool take_objective_into_account, bool ignore_trivial_constraints,
      const std::vector<std::pair<glop::RowIndex, double>>& lp_multipliers,
      IntegerValue* scaling,
      int64_t overflow_cap = std::numeric_limits<int64_t>::max()) const;

  // Can we have an overflow if we scale each coefficients with
  // std::round(std::ldexp(coeff, power)) ?
  bool ScalingCanOverflow(
      int power, bool take_objective_into_account,
      absl::Span<const std::pair<glop::RowIndex, double>> multipliers,
      int64_t overflow_cap) const;

  // Computes from an integer linear combination of the integer rows of the LP a
  // new constraint of the form "sum terms <= upper_bound". All computation are
  // exact here.
  //
  // Returns false if we encountered any integer overflow. If the template bool
  // is false, we do not check for a bit of extra speed.
  template <bool check_overflow = true>
  bool ComputeNewLinearConstraint(
      absl::Span<const std::pair<glop::RowIndex, IntegerValue>>
          integer_multipliers,
      ScatteredIntegerVector* scattered_vector,
      IntegerValue* upper_bound) const;

  // Simple heuristic to try to minimize |upper_bound - ImpliedLB(terms)|. This
  // should make the new constraint tighter and correct a bit the imprecision
  // introduced by rounding the floating points values.
  void AdjustNewLinearConstraint(
      std::vector<std::pair<glop::RowIndex, IntegerValue>>* integer_multipliers,
      ScatteredIntegerVector* scattered_vector,
      IntegerValue* upper_bound) const;

  // Shortcut for an integer linear expression type.
  using LinearExpression = std::vector<std::pair<glop::ColIndex, IntegerValue>>;

  // Converts a dense representation of a linear constraint to a sparse one
  // expressed in terms of IntegerVariable.
  void ConvertToLinearConstraint(
      const util_intops::StrongVector<glop::ColIndex, IntegerValue>&
          dense_vector,
      IntegerValue upper_bound, LinearConstraint* result);

  // Compute the implied lower bound of the given linear expression using the
  // current variable bound.
  absl::int128 GetImpliedLowerBound(const LinearConstraint& terms) const;

  // Fills the deductions vector with reduced cost deductions that can be made
  // from the current state of the LP solver. The given delta should be the
  // difference between the cp objective upper bound and lower bound given by
  // the lp.
  void ReducedCostStrengtheningDeductions(double cp_objective_delta);

  // Returns the variable value on the same scale as the CP variable value.
  glop::Fractional GetVariableValueAtCpScale(glop::ColIndex var);

  // Gets an LP variable that mirrors a CP variable.
  // The variable should be a positive reference.
  glop::ColIndex GetMirrorVariable(IntegerVariable positive_variable);

  // This must be called on an OPTIMAL LP and will update the data for
  // LPReducedCostAverageDecision().
  void UpdateAverageReducedCosts();

  // Callback underlying LPReducedCostAverageBranching().
  IntegerLiteral LPReducedCostAverageDecision();

  // Updates the simplex iteration limit for the next visit.
  // As per current algorithm, we use a limit which is dependent on size of the
  // problem and drop it significantly if degeneracy is detected. We use
  // DUAL_FEASIBLE status as a signal to correct the prediction. The next limit
  // is capped by 'min_iter' and 'max_iter'. Note that this is enabled only for
  // linearization level 2 and above.
  void UpdateSimplexIterationLimit(int64_t min_iter, int64_t max_iter);

  // Returns the col/coeff of integer_lp_[row].
  absl::Span<const glop::ColIndex> IntegerLpRowCols(glop::RowIndex row) const;
  absl::Span<const IntegerValue> IntegerLpRowCoeffs(glop::RowIndex row) const;

  // This epsilon is related to the precision of the value/reduced_cost returned
  // by the LP once they have been scaled back into the CP domain. So for large
  // domain or cost coefficient, we may have some issues.
  static constexpr double kCpEpsilon = 1e-4;

  // Same but at the LP scale.
  static constexpr double kLpEpsilon = 1e-6;

  // Anything coming from the LP with a magnitude below that will be assumed to
  // be zero.
  static constexpr double kZeroTolerance = 1e-12;

  // Class responsible for managing all possible constraints that may be part
  // of the LP.
  LinearConstraintManager constraint_manager_;

  // We do not want to add too many cut during each generation round.
  TopNCuts top_n_cuts_ = TopNCuts(10);

  // Initial problem in integer form.
  // We always sort the inner vectors by increasing glop::ColIndex.
  struct LinearConstraintInternal {
    IntegerValue lb;
    IntegerValue ub;

    // Point in integer_lp_cols_/integer_lp_coeffs_ for the actual data.
    int start_in_buffer;
    int num_terms;

    bool lb_is_trivial = false;
    bool ub_is_trivial = false;
  };
  std::vector<glop::ColIndex> integer_lp_cols_;
  std::vector<IntegerValue> integer_lp_coeffs_;

  std::vector<glop::ColIndex> tmp_cols_;
  std::vector<IntegerValue> tmp_coeffs_;

  LinearExpression integer_objective_;
  IntegerValue integer_objective_offset_ = IntegerValue(0);
  IntegerValue objective_infinity_norm_ = IntegerValue(0);
  util_intops::StrongVector<glop::RowIndex, LinearConstraintInternal>
      integer_lp_;
  util_intops::StrongVector<glop::RowIndex, IntegerValue> infinity_norms_;

  // Underlying LP solver API.
  glop::GlopParameters simplex_params_;
  glop::BasisState state_;
  glop::LinearProgram lp_data_;
  glop::RevisedSimplex simplex_;
  int64_t next_simplex_iter_ = 500;

  // For the scaling.
  glop::LpScalingHelper scaler_;

  // Temporary data for cuts.
  ZeroHalfCutHelper zero_half_cut_helper_;
  CoverCutHelper cover_cut_helper_;
  IntegerRoundingCutHelper integer_rounding_cut_helper_;

  bool problem_proven_infeasible_by_cuts_ = false;
  CutData base_ct_;

  ScatteredIntegerVector tmp_scattered_vector_;

  std::vector<double> tmp_lp_values_;
  std::vector<IntegerValue> tmp_var_lbs_;
  std::vector<IntegerValue> tmp_var_ubs_;
  std::vector<glop::RowIndex> tmp_slack_rows_;
  std::vector<std::pair<glop::ColIndex, IntegerValue>> tmp_terms_;

  // Used by AddCGCuts().
  std::vector<std::pair<glop::RowIndex, double>> tmp_lp_multipliers_;
  std::vector<std::pair<glop::RowIndex, IntegerValue>> tmp_integer_multipliers_;

  // Used by ScaleLpMultiplier().
  mutable std::vector<std::pair<glop::RowIndex, double>> tmp_cp_multipliers_;

  // Structures used for mirroring IntegerVariables inside the underlying LP
  // solver: an integer variable var is mirrored by mirror_lp_variable_[var].
  // Note that these indices are dense in [0, mirror_lp_variable_.size()] so
  // they can be used as vector indices.
  //
  // TODO(user): This should be util_intops::StrongVector<glop::ColIndex,
  // IntegerVariable> Except if we have too many LinearProgrammingConstraint.
  std::vector<IntegerVariable> integer_variables_;
  absl::flat_hash_map<IntegerVariable, glop::ColIndex> mirror_lp_variable_;

  // We need to remember what to optimize if an objective is given, because
  // then we will switch the objective between feasibility and optimization.
  bool objective_is_defined_ = false;
  IntegerVariable objective_cp_;

  // Singletons from Model.
  //
  // TODO(user): ObjectiveDefinition and SharedResponseManager are only needed
  // to report the objective bounds during propagation, find a better way to
  // avoid some of these dependencies?
  const SatParameters& parameters_;
  Model* model_;
  TimeLimit* time_limit_;
  IntegerTrail* integer_trail_;
  Trail* trail_;
  GenericLiteralWatcher* watcher_;
  IntegerEncoder* integer_encoder_;
  ProductDetector* product_detector_;
  ObjectiveDefinition* objective_definition_;
  SharedStatistics* shared_stats_;
  SharedResponseManager* shared_response_manager_;
  ModelRandomGenerator* random_;

  int watcher_id_;

  BoolRLTCutHelper rlt_cut_helper_;

  // Used while deriving cuts.
  ImpliedBoundsProcessor implied_bounds_processor_;

  // The dispatcher for all LP propagators of the model, allows to find which
  // LinearProgrammingConstraint has a given IntegerVariable.
  LinearProgrammingDispatcher* dispatcher_;

  std::vector<IntegerLiteral> integer_reason_;
  std::vector<IntegerLiteral> deductions_;
  std::vector<IntegerLiteral> deductions_reason_;

  // Repository of IntegerSumLE128 that needs to be kept around for the lazy
  // reasons. Those are new integer constraint that are created each time we
  // solve the LP to a dual-feasible solution. Propagating these constraints
  // both improve the objective lower bound but also perform reduced cost
  // fixing.
  int rev_optimal_constraints_size_ = 0;
  std::vector<std::unique_ptr<IntegerSumLE128>> optimal_constraints_;
  std::vector<int64_t> cumulative_optimal_constraint_sizes_;

  // Last OPTIMAL solution found by a call to the underlying LP solver.
  // On IncrementalPropagate(), if the bound updates do not invalidate this
  // solution, Propagate() will not find domain reductions, no need to call it.
  int lp_solution_level_ = 0;
  bool lp_solution_is_set_ = false;
  bool lp_solution_is_integer_ = false;
  std::vector<double> lp_solution_;
  std::vector<double> lp_reduced_cost_;

  // Last objective lower bound found by the LP Solver.
  // We erase this on backtrack.
  int previous_level_ = 0;
  bool lp_at_optimal_ = false;
  double lp_objective_lower_bound_;

  // If non-empty, this is the last known optimal lp solution at root-node. If
  // the variable bounds changed, or cuts where added, it is possible that this
  // solution is no longer optimal though.
  std::vector<double> level_zero_lp_solution_;

  // True if the last time we solved the exact same LP at level zero, no cuts
  // and no lazy constraints where added.
  bool lp_at_level_zero_is_final_ = false;

  // Same as lp_solution_ but this vector is indexed by IntegerVariable.
  ModelLpValues& expanded_lp_solution_;

  // Linear constraints cannot be created or modified after this is registered.
  bool lp_constraint_is_registered_ = false;

  std::vector<CutGenerator> cut_generators_;

  // Store some statistics for HeuristicLPReducedCostAverage().
  bool compute_reduced_cost_averages_ = false;
  int num_calls_since_reduced_cost_averages_reset_ = 0;
  std::vector<double> sum_cost_up_;
  std::vector<double> sum_cost_down_;
  std::vector<int> num_cost_up_;
  std::vector<int> num_cost_down_;
  std::vector<double> rc_scores_;

  // All the entries before rev_rc_start_ in the sorted positions correspond
  // to fixed variables and can be ignored.
  int rev_rc_start_ = 0;
  RevRepository<int> rc_rev_int_repository_;
  std::vector<std::pair<double, int>> positions_by_decreasing_rc_score_;

  // Defined as average number of nonbasic variables with zero reduced costs.
  IncrementalAverage average_degeneracy_;
  bool is_degenerate_ = false;

  // Sum of all simplex iterations performed by this class. This is useful to
  // test the incrementality and compare to other solvers.
  int64_t total_num_simplex_iterations_ = 0;

  // As we form candidate form cuts, sometimes we can propagate level zero
  // bounds with them.
  FirstFewValues<10> reachable_;
  int64_t total_num_cut_propagations_ = 0;
  int64_t total_num_eq_propagations_ = 0;

  // The number of times we changed the LP.
  int64_t num_lp_changes_ = 0;

  // Some stats on the LP statuses encountered.
  int64_t num_solves_ = 0;
  mutable int64_t num_adjusts_ = 0;
  mutable int64_t num_cut_overflows_ = 0;
  mutable int64_t num_bad_cuts_ = 0;
  mutable int64_t num_scaling_issues_ = 0;
  std::vector<int64_t> num_solves_by_status_;

  // We might temporarily disable the LP propagation.
  bool enabled_ = true;
};

// A class that stores which LP propagator is associated to each variable.
// We need to give the hash_map a name so it can be used as a singleton in our
// model.
//
// Important: only positive variable do appear here.
class LinearProgrammingDispatcher
    : public absl::flat_hash_map<IntegerVariable,
                                 LinearProgrammingConstraint*> {};

// A class that stores the collection of all LP constraints in a model.
class LinearProgrammingConstraintCollection
    : public std::vector<LinearProgrammingConstraint*> {
 public:
  explicit LinearProgrammingConstraintCollection(Model* model)
      : std::vector<LinearProgrammingConstraint*>() {
    model->GetOrCreate<CpSolverResponseStatisticCallbacks>()
        ->callbacks.push_back([this](CpSolverResponse* response) {
          int64_t num_lp_iters = 0;
          for (const LinearProgrammingConstraint* lp : *this) {
            num_lp_iters += lp->total_num_simplex_iterations();
          }
          response->set_num_lp_iterations(num_lp_iters);
        });
  }
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LINEAR_PROGRAMMING_CONSTRAINT_H_
