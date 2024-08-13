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

#include "ortools/sat/linear_programming_constraint.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/numeric/int128.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/algorithms/binary_search.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/strong_vector.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/revised_simplex.h"
#include "ortools/glop/status.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_data_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/scattered_vector.h"
#include "ortools/lp_data/sparse_column.h"
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
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/sat/zero_half_cuts.h"
#include "ortools/util/bitset.h"
#include "ortools/util/rev.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

using glop::ColIndex;
using glop::Fractional;
using glop::RowIndex;

void ScatteredIntegerVector::ClearAndResize(int size) {
  if (is_sparse_) {
    for (const glop::ColIndex col : non_zeros_) {
      dense_vector_[col] = IntegerValue(0);
    }
    dense_vector_.resize(size, IntegerValue(0));
  } else {
    dense_vector_.assign(size, IntegerValue(0));
  }
  for (const glop::ColIndex col : non_zeros_) {
    is_zeros_[col] = true;
  }
  is_zeros_.resize(size, true);
  non_zeros_.clear();
  is_sparse_ = true;
}

bool ScatteredIntegerVector::Add(glop::ColIndex col, IntegerValue value) {
  const int64_t add = CapAdd(value.value(), dense_vector_[col].value());
  if (add == std::numeric_limits<int64_t>::min() ||
      add == std::numeric_limits<int64_t>::max())
    return false;
  dense_vector_[col] = IntegerValue(add);
  if (is_sparse_ && is_zeros_[col]) {
    is_zeros_[col] = false;
    non_zeros_.push_back(col);
  }
  return true;
}

template <bool check_overflow>
bool ScatteredIntegerVector::AddLinearExpressionMultiple(
    const IntegerValue multiplier, absl::Span<const glop::ColIndex> cols,
    absl::Span<const IntegerValue> coeffs) {
  const double threshold = 0.1 * static_cast<double>(dense_vector_.size());
  const int num_terms = cols.size();
  if (is_sparse_ && static_cast<double>(num_terms) < threshold) {
    for (int i = 0; i < num_terms; ++i) {
      if (is_zeros_[cols[i]]) {
        is_zeros_[cols[i]] = false;
        non_zeros_.push_back(cols[i]);
      }
      if (check_overflow) {
        if (!AddProductTo(multiplier, coeffs[i], &dense_vector_[cols[i]])) {
          return false;
        }
      } else {
        dense_vector_[cols[i]] += multiplier * coeffs[i];
      }
    }
    if (static_cast<double>(non_zeros_.size()) > threshold) {
      is_sparse_ = false;
    }
  } else {
    is_sparse_ = false;
    for (int i = 0; i < num_terms; ++i) {
      if (check_overflow) {
        if (!AddProductTo(multiplier, coeffs[i], &dense_vector_[cols[i]])) {
          return false;
        }
      } else {
        dense_vector_[cols[i]] += multiplier * coeffs[i];
      }
    }
  }
  return true;
}

LinearConstraint ScatteredIntegerVector::ConvertToLinearConstraint(
    absl::Span<const IntegerVariable> integer_variables,
    IntegerValue upper_bound,
    std::optional<std::pair<IntegerVariable, IntegerValue>> extra_term) {
  // We first do one pass to compute the exact size and not overallocate.
  int final_size = 0;
  if (is_sparse_) {
    for (const glop::ColIndex col : non_zeros_) {
      const IntegerValue coeff = dense_vector_[col];
      if (coeff == 0) continue;
      ++final_size;
    }
  } else {
    for (const IntegerValue coeff : dense_vector_) {
      if (coeff != 0) ++final_size;
    }
  }
  if (extra_term != std::nullopt) ++final_size;

  // Allocate once.
  LinearConstraint result;
  result.resize(final_size);

  // Copy terms.
  int new_size = 0;
  if (is_sparse_) {
    std::sort(non_zeros_.begin(), non_zeros_.end());
    for (const glop::ColIndex col : non_zeros_) {
      const IntegerValue coeff = dense_vector_[col];
      if (coeff == 0) continue;
      result.vars[new_size] = integer_variables[col.value()];
      result.coeffs[new_size] = coeff;
      ++new_size;
    }
  } else {
    const int size = dense_vector_.size();
    for (glop::ColIndex col(0); col < size; ++col) {
      const IntegerValue coeff = dense_vector_[col];
      if (coeff == 0) continue;
      result.vars[new_size] = integer_variables[col.value()];
      result.coeffs[new_size] = coeff;
      ++new_size;
    }
  }

  result.lb = kMinIntegerValue;
  result.ub = upper_bound;

  if (extra_term != std::nullopt) {
    result.vars[new_size] += extra_term->first;
    result.coeffs[new_size] += extra_term->second;
    ++new_size;
  }

  CHECK_EQ(new_size, final_size);
  DivideByGCD(&result);
  return result;
}

void ScatteredIntegerVector::ConvertToCutData(
    absl::int128 rhs, absl::Span<const IntegerVariable> integer_variables,
    absl::Span<const double> lp_solution, IntegerTrail* integer_trail,
    CutData* result) {
  result->terms.clear();
  result->rhs = rhs;
  if (is_sparse_) {
    std::sort(non_zeros_.begin(), non_zeros_.end());
    for (const glop::ColIndex col : non_zeros_) {
      const IntegerValue coeff = dense_vector_[col];
      if (coeff == 0) continue;
      const IntegerVariable var = integer_variables[col.value()];
      CHECK(result->AppendOneTerm(var, coeff, lp_solution[col.value()],
                                  integer_trail->LevelZeroLowerBound(var),
                                  integer_trail->LevelZeroUpperBound(var)));
    }
  } else {
    const int size = dense_vector_.size();
    for (glop::ColIndex col(0); col < size; ++col) {
      const IntegerValue coeff = dense_vector_[col];
      if (coeff == 0) continue;
      const IntegerVariable var = integer_variables[col.value()];
      CHECK(result->AppendOneTerm(var, coeff, lp_solution[col.value()],
                                  integer_trail->LevelZeroLowerBound(var),
                                  integer_trail->LevelZeroUpperBound(var)));
    }
  }
}

std::vector<std::pair<glop::ColIndex, IntegerValue>>
ScatteredIntegerVector::GetTerms() {
  std::vector<std::pair<glop::ColIndex, IntegerValue>> result;
  if (is_sparse_) {
    std::sort(non_zeros_.begin(), non_zeros_.end());
    for (const glop::ColIndex col : non_zeros_) {
      const IntegerValue coeff = dense_vector_[col];
      if (coeff != 0) result.push_back({col, coeff});
    }
  } else {
    const int size = dense_vector_.size();
    for (glop::ColIndex col(0); col < size; ++col) {
      const IntegerValue coeff = dense_vector_[col];
      if (coeff != 0) result.push_back({col, coeff});
    }
  }
  return result;
}

// TODO(user): make SatParameters singleton too, otherwise changing them after
// a constraint was added will have no effect on this class.
LinearProgrammingConstraint::LinearProgrammingConstraint(
    Model* model, absl::Span<const IntegerVariable> vars)
    : constraint_manager_(model),
      parameters_(*(model->GetOrCreate<SatParameters>())),
      model_(model),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      trail_(model->GetOrCreate<Trail>()),
      watcher_(model->GetOrCreate<GenericLiteralWatcher>()),
      integer_encoder_(model->GetOrCreate<IntegerEncoder>()),
      product_detector_(model->GetOrCreate<ProductDetector>()),
      objective_definition_(model->GetOrCreate<ObjectiveDefinition>()),
      shared_stats_(model->GetOrCreate<SharedStatistics>()),
      shared_response_manager_(model->GetOrCreate<SharedResponseManager>()),
      random_(model->GetOrCreate<ModelRandomGenerator>()),
      rlt_cut_helper_(model),
      implied_bounds_processor_({}, integer_trail_,
                                model->GetOrCreate<ImpliedBounds>()),
      dispatcher_(model->GetOrCreate<LinearProgrammingDispatcher>()),
      expanded_lp_solution_(*model->GetOrCreate<ModelLpValues>()) {
  // Tweak the default parameters to make the solve incremental.
  simplex_params_.set_use_dual_simplex(true);
  simplex_params_.set_cost_scaling(glop::GlopParameters::MEAN_COST_SCALING);
  simplex_params_.set_primal_feasibility_tolerance(
      parameters_.lp_primal_tolerance());
  simplex_params_.set_dual_feasibility_tolerance(
      parameters_.lp_dual_tolerance());
  if (parameters_.use_exact_lp_reason()) {
    simplex_params_.set_change_status_to_imprecise(false);
  }
  simplex_.SetParameters(simplex_params_);
  if (parameters_.search_branching() == SatParameters::LP_SEARCH) {
    compute_reduced_cost_averages_ = true;
  }

  // Register our local rev int repository.
  integer_trail_->RegisterReversibleClass(&rc_rev_int_repository_);

  // Register SharedStatistics with the cut helpers.
  auto* stats = model->GetOrCreate<SharedStatistics>();
  integer_rounding_cut_helper_.SetSharedStatistics(stats);
  cover_cut_helper_.SetSharedStatistics(stats);

  // Initialize the IntegerVariable -> ColIndex mapping.
  CHECK(std::is_sorted(vars.begin(), vars.end()));

  integer_variables_.assign(vars.begin(), vars.end());
  ColIndex col{0};
  for (const IntegerVariable positive_variable : vars) {
    CHECK(VariableIsPositive(positive_variable));
    implied_bounds_processor_.AddLpVariable(positive_variable);
    (*dispatcher_)[positive_variable] = this;
    mirror_lp_variable_[positive_variable] = col;

    ++col;
  }
  lp_solution_.assign(vars.size(), std::numeric_limits<double>::infinity());
  lp_reduced_cost_.assign(vars.size(), 0.0);

  if (!vars.empty()) {
    const int max_index = NegationOf(vars.back()).value();
    if (max_index >= expanded_lp_solution_.size()) {
      expanded_lp_solution_.assign(max_index + 1, 0.0);
    }
  }
}

void LinearProgrammingConstraint::AddLinearConstraint(LinearConstraint ct) {
  DCHECK(!lp_constraint_is_registered_);
  constraint_manager_.Add(std::move(ct));
}

glop::ColIndex LinearProgrammingConstraint::GetMirrorVariable(
    IntegerVariable positive_variable) {
  DCHECK(VariableIsPositive(positive_variable));
  return mirror_lp_variable_.at(positive_variable);
}

void LinearProgrammingConstraint::SetObjectiveCoefficient(IntegerVariable ivar,
                                                          IntegerValue coeff) {
  CHECK(!lp_constraint_is_registered_);
  objective_is_defined_ = true;
  IntegerVariable pos_var = VariableIsPositive(ivar) ? ivar : NegationOf(ivar);
  if (ivar != pos_var) coeff = -coeff;

  constraint_manager_.SetObjectiveCoefficient(pos_var, coeff);
  const glop::ColIndex col = GetMirrorVariable(pos_var);
  integer_objective_.push_back({col, coeff});
  objective_infinity_norm_ =
      std::max(objective_infinity_norm_, IntTypeAbs(coeff));
}

// TODO(user): As the search progress, some variables might get fixed. Exploit
// this to reduce the number of variables in the LP and in the
// ConstraintManager? We might also detect during the search that two variable
// are equivalent.
//
// TODO(user): On TSP/VRP with a lot of cuts, this can take 20% of the overall
// running time. We should be able to almost remove most of this from the
// profile by being more incremental (modulo LP scaling).
//
// TODO(user): A longer term idea for LP with a lot of variables is to not
// add all variables to each LP solve and do some "sifting". That can be useful
// for TSP for instance where the number of edges is large, but only a small
// fraction will be used in the optimal solution.
bool LinearProgrammingConstraint::CreateLpFromConstraintManager() {
  simplex_.NotifyThatMatrixIsChangedForNextSolve();

  // Fill integer_lp_.
  integer_lp_.clear();
  integer_lp_cols_.clear();
  integer_lp_coeffs_.clear();

  infinity_norms_.clear();
  const auto& all_constraints = constraint_manager_.AllConstraints();
  for (const auto index : constraint_manager_.LpConstraints()) {
    const LinearConstraint& ct = all_constraints[index].constraint;

    integer_lp_.push_back(LinearConstraintInternal());
    LinearConstraintInternal& new_ct = integer_lp_.back();
    new_ct.lb = ct.lb;
    new_ct.ub = ct.ub;
    new_ct.lb_is_trivial = all_constraints[index].lb_is_trivial;
    new_ct.ub_is_trivial = all_constraints[index].ub_is_trivial;
    const int size = ct.num_terms;
    if (ct.lb > ct.ub) {
      VLOG(1) << "Trivial infeasible bound in an LP constraint";
      return false;
    }

    IntegerValue infinity_norm = 0;
    infinity_norm = std::max(infinity_norm, IntTypeAbs(ct.lb));
    infinity_norm = std::max(infinity_norm, IntTypeAbs(ct.ub));
    new_ct.start_in_buffer = integer_lp_cols_.size();
    new_ct.num_terms = size;
    for (int i = 0; i < size; ++i) {
      // We only use positive variable inside this class.
      const IntegerVariable var = ct.vars[i];
      const IntegerValue coeff = ct.coeffs[i];
      infinity_norm = std::max(infinity_norm, IntTypeAbs(coeff));
      integer_lp_cols_.push_back(GetMirrorVariable(var));
      integer_lp_coeffs_.push_back(coeff);
    }
    infinity_norms_.push_back(infinity_norm);

    // It is important to keep lp_data_ "clean".
    DCHECK(std::is_sorted(
        integer_lp_cols_.data() + new_ct.start_in_buffer,
        integer_lp_cols_.data() + new_ct.start_in_buffer + new_ct.num_terms));
  }

  // Copy the integer_lp_ into lp_data_.
  lp_data_.Clear();
  for (int i = 0; i < integer_variables_.size(); ++i) {
    CHECK_EQ(glop::ColIndex(i), lp_data_.CreateNewVariable());
  }

  // We remove fixed variables from the objective. This should help the LP
  // scaling, but also our integer reason computation.
  int new_size = 0;
  objective_infinity_norm_ = 0;
  for (const auto& entry : integer_objective_) {
    const IntegerVariable var = integer_variables_[entry.first.value()];
    if (integer_trail_->IsFixedAtLevelZero(var)) {
      integer_objective_offset_ +=
          entry.second * integer_trail_->LevelZeroLowerBound(var);
      continue;
    }
    objective_infinity_norm_ =
        std::max(objective_infinity_norm_, IntTypeAbs(entry.second));
    integer_objective_[new_size++] = entry;
    lp_data_.SetObjectiveCoefficient(entry.first, ToDouble(entry.second));
  }
  objective_infinity_norm_ =
      std::max(objective_infinity_norm_, IntTypeAbs(integer_objective_offset_));
  integer_objective_.resize(new_size);
  lp_data_.SetObjectiveOffset(ToDouble(integer_objective_offset_));

  for (const LinearConstraintInternal& ct : integer_lp_) {
    const ConstraintIndex row = lp_data_.CreateNewConstraint();

    // TODO(user): Using trivial bound might be good for things like
    // sum bool <= 1 since setting the slack in [0, 1] can lead to bound flip in
    // the simplex. However if the bound is large, maybe it make more sense to
    // use +/- infinity.
    const double infinity = std::numeric_limits<double>::infinity();
    lp_data_.SetConstraintBounds(
        row, ct.lb_is_trivial ? -infinity : ToDouble(ct.lb),
        ct.ub_is_trivial ? +infinity : ToDouble(ct.ub));
    for (int i = 0; i < ct.num_terms; ++i) {
      const int index = ct.start_in_buffer + i;
      lp_data_.SetCoefficient(row, integer_lp_cols_[index],
                              ToDouble(integer_lp_coeffs_[index]));
    }
  }
  lp_data_.NotifyThatColumnsAreClean();

  // We scale the LP using the level zero bounds that we later override
  // with the current ones.
  //
  // TODO(user): As part of the scaling, we may also want to shift the initial
  // variable bounds so that each variable contain the value zero in their
  // domain. Maybe just once and for all at the beginning.
  const int num_vars = integer_variables_.size();
  for (int i = 0; i < num_vars; i++) {
    const IntegerVariable cp_var = integer_variables_[i];
    const double lb = ToDouble(integer_trail_->LevelZeroLowerBound(cp_var));
    const double ub = ToDouble(integer_trail_->LevelZeroUpperBound(cp_var));
    lp_data_.SetVariableBounds(glop::ColIndex(i), lb, ub);
  }

  // TODO(user): As we have an idea of the LP optimal after the first solves,
  // maybe we can adapt the scaling accordingly.
  scaler_.Scale(simplex_params_, &lp_data_);
  UpdateBoundsOfLpVariables();

  // Set the information for the step to polish the LP basis. All our variables
  // are integer, but for now, we just try to minimize the fractionality of the
  // binary variables.
  if (parameters_.polish_lp_solution()) {
    simplex_.ClearIntegralityScales();
    for (int i = 0; i < num_vars; ++i) {
      const IntegerVariable cp_var = integer_variables_[i];
      const IntegerValue lb = integer_trail_->LevelZeroLowerBound(cp_var);
      const IntegerValue ub = integer_trail_->LevelZeroUpperBound(cp_var);
      if (lb != 0 || ub != 1) continue;
      simplex_.SetIntegralityScale(
          glop::ColIndex(i),
          1.0 / scaler_.VariableScalingFactor(glop::ColIndex(i)));
    }
  }

  lp_data_.NotifyThatColumnsAreClean();
  VLOG(3) << "LP relaxation: " << lp_data_.GetDimensionString() << ". "
          << constraint_manager_.AllConstraints().size()
          << " Managed constraints.";
  return true;
}

void LinearProgrammingConstraint::FillReducedCostReasonIn(
    const glop::DenseRow& reduced_costs,
    std::vector<IntegerLiteral>* integer_reason) {
  integer_reason->clear();
  const int num_vars = integer_variables_.size();
  for (int i = 0; i < num_vars; i++) {
    const double rc = reduced_costs[glop::ColIndex(i)];
    if (rc > kLpEpsilon) {
      integer_reason->push_back(
          integer_trail_->LowerBoundAsLiteral(integer_variables_[i]));
    } else if (rc < -kLpEpsilon) {
      integer_reason->push_back(
          integer_trail_->UpperBoundAsLiteral(integer_variables_[i]));
    }
  }

  integer_trail_->RemoveLevelZeroBounds(integer_reason);
}

void LinearProgrammingConstraint::RegisterWith(Model* model) {
  DCHECK(!lp_constraint_is_registered_);
  lp_constraint_is_registered_ = true;
  model->GetOrCreate<LinearProgrammingConstraintCollection>()->push_back(this);

  // Note fdid, this is not really needed by should lead to better cache
  // locality.
  std::sort(integer_objective_.begin(), integer_objective_.end());

  // Set the LP to its initial content.
  //
  // Note that we always add LP constraint lazily if we have A LOT of them.
  // This is because currently on large problem with millions of constraints,
  // our LP is usually not fast enough anyway.
  if (!parameters_.add_lp_constraints_lazily() &&
      constraint_manager_.num_constraints() < 1e6) {
    constraint_manager_.AddAllConstraintsToLp();
  }
  if (!CreateLpFromConstraintManager()) {
    model->GetOrCreate<SatSolver>()->NotifyThatModelIsUnsat();
    return;
  }

  watcher_id_ = watcher_->Register(this);
  const int num_vars = integer_variables_.size();
  for (int i = 0; i < num_vars; i++) {
    watcher_->WatchIntegerVariable(integer_variables_[i], watcher_id_, i);
  }
  if (objective_is_defined_) {
    watcher_->WatchUpperBound(objective_cp_, watcher_id_);
  }
  watcher_->SetPropagatorPriority(watcher_id_, 2);
  watcher_->AlwaysCallAtLevelZero(watcher_id_);

  // Registering it with the trail make sure this class is always in sync when
  // it is used in the decision heuristics.
  integer_trail_->RegisterReversibleClass(this);
  watcher_->RegisterReversibleInt(watcher_id_, &rev_optimal_constraints_size_);
}

void LinearProgrammingConstraint::SetLevel(int level) {
  // Get rid of all optimal constraint each time we go back to level zero.
  if (level == 0) rev_optimal_constraints_size_ = 0;
  optimal_constraints_.resize(rev_optimal_constraints_size_);
  cumulative_optimal_constraint_sizes_.resize(rev_optimal_constraints_size_);
  if (lp_solution_is_set_ && level < lp_solution_level_) {
    lp_solution_is_set_ = false;
  }

  if (level < previous_level_) {
    lp_at_optimal_ = false;
    lp_objective_lower_bound_ = -std::numeric_limits<double>::infinity();
  }
  previous_level_ = level;

  // Special case for level zero, we "reload" any previously known optimal
  // solution from that level.
  //
  // TODO(user): Keep all optimal solution in the current branch?
  // TODO(user): Still try to add cuts/constraints though!
  // TODO(user): Reload the basis? This might cause issue with the basis
  // saving/loading code in lb_tree_search.
  if (level == 0 && !lp_solution_is_set_ && !level_zero_lp_solution_.empty()) {
    lp_solution_is_set_ = true;
    lp_solution_ = level_zero_lp_solution_;
    lp_solution_level_ = 0;
    for (int i = 0; i < lp_solution_.size(); i++) {
      expanded_lp_solution_[integer_variables_[i]] = lp_solution_[i];
      expanded_lp_solution_[NegationOf(integer_variables_[i])] =
          -lp_solution_[i];
    }
  }
}

void LinearProgrammingConstraint::AddCutGenerator(CutGenerator generator) {
  cut_generators_.push_back(std::move(generator));
}

bool LinearProgrammingConstraint::IncrementalPropagate(
    const std::vector<int>& watch_indices) {
  if (!enabled_) return true;

  // If we have a really deep branch, with a lot of LP explanation constraint,
  // we could take a quadratic amount of memory: O(num_var) per number of
  // propagation in that branch. To avoid that, once the memory starts to be
  // over a few GB, we only propagate from time to time. This way we do not need
  // to keep that many constraints around.
  //
  // Note that 100 Millions int32_t variables, with the int128 coefficients and
  // extra propagation vector is already a few GB.
  if (!cumulative_optimal_constraint_sizes_.empty()) {
    const double current_size =
        static_cast<double>(cumulative_optimal_constraint_sizes_.back());
    const double low_limit = 1e7;
    if (current_size > low_limit) {
      // We only propagate if we use less that 100 times the number of current
      // integer literal enqueued.
      const double num_enqueues = static_cast<double>(integer_trail_->Index());
      if ((current_size - low_limit) > 100 * num_enqueues) return true;
    }
  }

  if (!lp_solution_is_set_) {
    return Propagate();
  }

  // At level zero, if there is still a chance to add cuts or lazy constraints,
  // we re-run the LP.
  if (trail_->CurrentDecisionLevel() == 0 && !lp_at_level_zero_is_final_) {
    return Propagate();
  }

  // Check whether the change breaks the current LP solution. If it does, call
  // Propagate() on the current LP.
  for (const int index : watch_indices) {
    const double lb =
        ToDouble(integer_trail_->LowerBound(integer_variables_[index]));
    const double ub =
        ToDouble(integer_trail_->UpperBound(integer_variables_[index]));
    const double value = lp_solution_[index];
    if (value < lb - kCpEpsilon || value > ub + kCpEpsilon) return Propagate();
  }

  // TODO(user): The saved lp solution is still valid given the current variable
  // bounds, so the LP optimal didn't change. However we might still want to add
  // new cuts or new lazy constraints?
  //
  // TODO(user): Propagate the last optimal_constraint? Note that we need
  // to be careful since the reversible int in IntegerSumLE are not registered.
  // However, because we delete "optimalconstraints" on backtrack, we might not
  // care.
  //
  // Remark: Note that if we do the sequence SetBasis() / Propagate() /
  // GetAndSaveBasis() and we are in the case where the solution is still
  // optimal, we should get the basis from when the lp solution was set which
  // should be what we want. Even if we set garbage during SetBasis() it should
  // be ignored. TODO(user): We might still have problem at level zero.
  return true;
}

glop::Fractional LinearProgrammingConstraint::GetVariableValueAtCpScale(
    glop::ColIndex var) {
  return scaler_.UnscaleVariableValue(var, simplex_.GetVariableValue(var));
}

double LinearProgrammingConstraint::GetSolutionValue(
    IntegerVariable variable) const {
  return lp_solution_[mirror_lp_variable_.at(variable).value()];
}

double LinearProgrammingConstraint::GetSolutionReducedCost(
    IntegerVariable variable) const {
  return lp_reduced_cost_[mirror_lp_variable_.at(variable).value()];
}

void LinearProgrammingConstraint::UpdateBoundsOfLpVariables() {
  const int num_vars = integer_variables_.size();
  for (int i = 0; i < num_vars; i++) {
    const IntegerVariable cp_var = integer_variables_[i];
    const double lb = ToDouble(integer_trail_->LowerBound(cp_var));
    const double ub = ToDouble(integer_trail_->UpperBound(cp_var));
    const double factor = scaler_.VariableScalingFactor(glop::ColIndex(i));
    lp_data_.SetVariableBounds(glop::ColIndex(i), lb * factor, ub * factor);
  }
}

bool LinearProgrammingConstraint::SolveLp() {
  const int level = trail_->CurrentDecisionLevel();
  if (level == 0) {
    lp_at_level_zero_is_final_ = false;
  }

  const auto status = simplex_.Solve(lp_data_, time_limit_);
  state_ = simplex_.GetState();
  total_num_simplex_iterations_ += simplex_.GetNumberOfIterations();
  if (!status.ok()) {
    VLOG(1) << "The LP solver encountered an error: " << status.error_message();
    simplex_.ClearStateForNextSolve();
    return false;
  }
  average_degeneracy_.AddData(CalculateDegeneracy());
  if (average_degeneracy_.CurrentAverage() >= 1000.0) {
    VLOG(2) << "High average degeneracy: "
            << average_degeneracy_.CurrentAverage();
  }

  // By default we assume the matrix is unchanged.
  // This will be reset by CreateLpFromConstraintManager().
  simplex_.NotifyThatMatrixIsUnchangedForNextSolve();

  const int status_as_int = static_cast<int>(simplex_.GetProblemStatus());
  if (status_as_int >= num_solves_by_status_.size()) {
    num_solves_by_status_.resize(status_as_int + 1);
  }
  num_solves_++;
  num_solves_by_status_[status_as_int]++;
  VLOG(2) << lp_data_.GetDimensionString()
          << " lvl:" << trail_->CurrentDecisionLevel() << " "
          << simplex_.GetProblemStatus()
          << " iter:" << simplex_.GetNumberOfIterations()
          << " obj:" << simplex_.GetObjectiveValue() << " scaled:"
          << objective_definition_->ScaleObjective(
                 simplex_.GetObjectiveValue());

  if (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL ||
      simplex_.GetProblemStatus() == glop::ProblemStatus::DUAL_FEASIBLE) {
    lp_objective_lower_bound_ = simplex_.GetObjectiveValue();
  }
  lp_at_optimal_ = simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL;

  if (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL) {
    lp_solution_is_set_ = true;
    lp_solution_level_ = trail_->CurrentDecisionLevel();
    const int num_vars = integer_variables_.size();
    for (int i = 0; i < num_vars; i++) {
      const glop::Fractional value =
          GetVariableValueAtCpScale(glop::ColIndex(i));
      lp_solution_[i] = value;
      expanded_lp_solution_[integer_variables_[i]] = value;
      expanded_lp_solution_[NegationOf(integer_variables_[i])] = -value;
    }

    if (lp_solution_level_ == 0) {
      level_zero_lp_solution_ = lp_solution_;
    }
  }

  return true;
}

bool LinearProgrammingConstraint::AnalyzeLp() {
  // A dual-unbounded problem is infeasible. We use the dual ray reason.
  if (simplex_.GetProblemStatus() == glop::ProblemStatus::DUAL_UNBOUNDED) {
    if (parameters_.use_exact_lp_reason()) {
      return PropagateExactDualRay();
    }
    FillReducedCostReasonIn(simplex_.GetDualRayRowCombination(),
                            &integer_reason_);
    return integer_trail_->ReportConflict(integer_reason_);
  }

  // TODO(user): Update limits for DUAL_UNBOUNDED status as well.
  UpdateSimplexIterationLimit(/*min_iter=*/10, /*max_iter=*/1000);

  // Optimality deductions if problem has an objective.
  // If there is no objective, then all duals will just be zero.
  if (objective_is_defined_ &&
      (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL ||
       simplex_.GetProblemStatus() == glop::ProblemStatus::DUAL_FEASIBLE)) {
    // TODO(user): Maybe do a bit less computation when we cannot propagate
    // anything.
    if (parameters_.use_exact_lp_reason()) {
      if (!PropagateExactLpReason()) return false;

      // Display when the inexact bound would have propagated more.
      if (VLOG_IS_ON(2)) {
        const double relaxed_optimal_objective = simplex_.GetObjectiveValue();
        const IntegerValue approximate_new_lb(static_cast<int64_t>(
            std::ceil(relaxed_optimal_objective - kCpEpsilon)));
        const IntegerValue propagated_lb =
            integer_trail_->LowerBound(objective_cp_);
        if (approximate_new_lb > propagated_lb) {
          VLOG(2) << "LP objective [ " << ToDouble(propagated_lb) << ", "
                  << ToDouble(integer_trail_->UpperBound(objective_cp_))
                  << " ] approx_lb += "
                  << ToDouble(approximate_new_lb - propagated_lb) << " gap: "
                  << integer_trail_->UpperBound(objective_cp_) - propagated_lb;
        }
      }
    } else {
      // Try to filter optimal objective value. Note that GetObjectiveValue()
      // already take care of the scaling so that it returns an objective in the
      // CP world.
      FillReducedCostReasonIn(simplex_.GetReducedCosts(), &integer_reason_);
      const double objective_cp_ub =
          ToDouble(integer_trail_->UpperBound(objective_cp_));
      const double relaxed_optimal_objective = simplex_.GetObjectiveValue();
      ReducedCostStrengtheningDeductions(objective_cp_ub -
                                         relaxed_optimal_objective);
      if (!deductions_.empty()) {
        deductions_reason_ = integer_reason_;
        deductions_reason_.push_back(
            integer_trail_->UpperBoundAsLiteral(objective_cp_));
      }

      // Push new objective lb.
      const IntegerValue approximate_new_lb(static_cast<int64_t>(
          std::ceil(relaxed_optimal_objective - kCpEpsilon)));
      if (approximate_new_lb > integer_trail_->LowerBound(objective_cp_)) {
        const IntegerLiteral deduction =
            IntegerLiteral::GreaterOrEqual(objective_cp_, approximate_new_lb);
        if (!integer_trail_->Enqueue(deduction, {}, integer_reason_)) {
          return false;
        }
      }

      // Push reduced cost strengthening bounds.
      if (!deductions_.empty()) {
        const int trail_index_with_same_reason = integer_trail_->Index();
        for (const IntegerLiteral deduction : deductions_) {
          if (!integer_trail_->Enqueue(deduction, {}, deductions_reason_,
                                       trail_index_with_same_reason)) {
            return false;
          }
        }
      }
    }
  }

  // Copy more info about the current solution.
  if (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL) {
    CHECK(lp_solution_is_set_);
    lp_solution_is_integer_ = true;
    const int num_vars = integer_variables_.size();
    for (int i = 0; i < num_vars; i++) {
      lp_reduced_cost_[i] = scaler_.UnscaleReducedCost(
          glop::ColIndex(i), simplex_.GetReducedCost(glop::ColIndex(i)));
      if (std::abs(lp_solution_[i] - std::round(lp_solution_[i])) >
          kCpEpsilon) {
        lp_solution_is_integer_ = false;
      }
    }

    if (compute_reduced_cost_averages_) {
      UpdateAverageReducedCosts();
    }
  }

  // On some problem, LP solves and cut rounds can be slow, so we report
  // the current possible objective improvement in the middle of the
  // propagation, not just at the end.
  //
  // Note that we currently only do that if the LP is "full" and its objective
  // is the same as the one of the whole problem.
  if (objective_is_defined_ &&
      objective_definition_->objective_var == objective_cp_ &&
      trail_->CurrentDecisionLevel() == 0) {
    shared_response_manager_->UpdateInnerObjectiveBounds(
        model_->Name(), integer_trail_->LowerBound(objective_cp_),
        integer_trail_->UpperBound(objective_cp_));
  }

  return true;
}

// Note that since we call this on the constraint with slack, we actually have
// linear expression == rhs, we can use this to propagate more!
//
// TODO(user): Also propagate on -cut ? in practice we already do that in many
// places were we try to generate the cut on -cut... But we coould do it sooner
// and more cleanly here.
bool LinearProgrammingConstraint::PreprocessCut(IntegerVariable first_slack,
                                                CutData* cut) {
  // Because of complement, all coeffs and all terms are positive after this.
  cut->ComplementForPositiveCoefficients();
  if (cut->rhs < 0) {
    problem_proven_infeasible_by_cuts_ = true;
    return false;
  }

  // Limited DP to compute first few reachable values.
  // Note that all coeff are positive.
  reachable_.Reset();
  for (const CutTerm& term : cut->terms) {
    reachable_.Add(term.coeff.value());
  }

  // Extra propag since we know it is actually an equality constraint.
  if (cut->rhs < absl::int128(reachable_.LastValue()) &&
      !reachable_.MightBeReachable(static_cast<int64_t>(cut->rhs))) {
    problem_proven_infeasible_by_cuts_ = true;
    return false;
  }

  bool some_fixed_terms = false;
  bool some_relevant_positions = false;
  for (CutTerm& term : cut->terms) {
    const absl::int128 magnitude128 = term.coeff.value();
    const absl::int128 range =
        absl::int128(term.bound_diff.value()) * magnitude128;

    IntegerValue new_diff = term.bound_diff;
    if (range > cut->rhs) {
      new_diff = static_cast<int64_t>(cut->rhs / magnitude128);
    }
    {
      // Extra propag since we know it is actually an equality constraint.
      absl::int128 rest128 =
          cut->rhs - absl::int128(new_diff.value()) * magnitude128;
      while (rest128 < absl::int128(reachable_.LastValue()) &&
             !reachable_.MightBeReachable(static_cast<int64_t>(rest128))) {
        ++total_num_eq_propagations_;
        CHECK_GT(new_diff, 0);
        --new_diff;
        rest128 += magnitude128;
      }
    }

    if (new_diff < term.bound_diff) {
      term.bound_diff = new_diff;

      const IntegerVariable var = term.expr_vars[0];
      if (var < first_slack) {
        // Normal variable.
        ++total_num_cut_propagations_;

        // Note that at this stage we only have X - lb or ub - X.
        if (term.expr_coeffs[0] == 1) {
          // X + offset <= bound_diff
          if (!integer_trail_->Enqueue(
                  IntegerLiteral::LowerOrEqual(
                      var, term.bound_diff - term.expr_offset),
                  {}, {})) {
            problem_proven_infeasible_by_cuts_ = true;
            return false;
          }
        } else {
          CHECK_EQ(term.expr_coeffs[0], -1);
          // offset - X <= bound_diff
          if (!integer_trail_->Enqueue(
                  IntegerLiteral::GreaterOrEqual(
                      var, term.expr_offset - term.bound_diff),
                  {}, {})) {
            problem_proven_infeasible_by_cuts_ = true;
            return false;
          }
        }
      } else {
        // This is a tighter bound on one of the constraint! like a cut. Note
        // that in some corner case, new cut can be merged and update the bounds
        // of the constraint before this code.
        const int slack_index = (var.value() - first_slack.value()) / 2;
        const glop::RowIndex row = tmp_slack_rows_[slack_index];
        if (term.expr_coeffs[0] == 1) {
          // slack = ct + offset <= bound_diff;
          const IntegerValue new_ub = term.bound_diff - term.expr_offset;
          if (constraint_manager_.UpdateConstraintUb(row, new_ub)) {
            integer_lp_[row].ub = new_ub;
          }
        } else {
          // slack = offset - ct <= bound_diff;
          CHECK_EQ(term.expr_coeffs[0], -1);
          const IntegerValue new_lb = term.expr_offset - term.bound_diff;
          if (constraint_manager_.UpdateConstraintLb(row, new_lb)) {
            integer_lp_[row].lb = new_lb;
          }
        }
      }
    }

    if (term.bound_diff == 0) {
      some_fixed_terms = true;
    } else {
      if (term.HasRelevantLpValue()) {
        some_relevant_positions = true;
      }
    }
  }

  // Remove fixed terms if any.
  if (some_fixed_terms) {
    int new_size = 0;
    for (const CutTerm& term : cut->terms) {
      if (term.bound_diff == 0) continue;
      cut->terms[new_size++] = term;
    }
    cut->terms.resize(new_size);
  }
  return some_relevant_positions;
}

bool LinearProgrammingConstraint::AddCutFromConstraints(
    absl::string_view name,
    absl::Span<const std::pair<RowIndex, IntegerValue>> integer_multipliers) {
  // This is initialized to a valid linear constraint (by taking linear
  // combination of the LP rows) and will be transformed into a cut if
  // possible.
  //
  // TODO(user): For CG cuts, Ideally this linear combination should have only
  // one fractional variable (basis_col). But because of imprecision, we can get
  // a bunch of fractional entry with small coefficient (relative to the one of
  // basis_col). We try to handle that in IntegerRoundingCut(), but it might be
  // better to add small multiple of the involved rows to get rid of them.
  IntegerValue cut_ub;
  if (!ComputeNewLinearConstraint(integer_multipliers, &tmp_scattered_vector_,
                                  &cut_ub)) {
    ++num_cut_overflows_;
    VLOG(1) << "Issue, overflow!";
    return false;
  }

  // Important: because we use integer_multipliers below to create the slack,
  // we cannot try to adjust this linear combination easily.
  //
  // TODO(user): the conversion col_index -> IntegerVariable is slow and could
  // in principle be removed. Easy for cuts, but not so much for
  // implied_bounds_processor_. Note that in theory this could allow us to
  // use Literal directly without the need to have an IntegerVariable for them.
  tmp_scattered_vector_.ConvertToCutData(cut_ub.value(), integer_variables_,
                                         lp_solution_, integer_trail_,
                                         &base_ct_);

  // If there are no integer (all Booleans), no need to try implied bounds
  // heurititics. By setting this to nullptr, we are a bit faster.
  ImpliedBoundsProcessor* ib_processor = nullptr;
  {
    bool some_ints = false;
    bool some_relevant_positions = false;
    for (const CutTerm& term : base_ct_.terms) {
      if (term.bound_diff > 1) some_ints = true;
      if (term.HasRelevantLpValue()) some_relevant_positions = true;
    }

    // If all value are integer, we will not be able to cut anything.
    if (!some_relevant_positions) return false;
    if (some_ints) ib_processor = &implied_bounds_processor_;
  }

  // Add constraint slack.
  // This is important for correctness here.
  const IntegerVariable first_slack(expanded_lp_solution_.size());
  CHECK_EQ(first_slack.value() % 2, 0);
  tmp_slack_rows_.clear();
  for (const auto [row, coeff] : integer_multipliers) {
    if (integer_lp_[row].lb == integer_lp_[row].ub) continue;

    CutTerm entry;
    entry.coeff = coeff > 0 ? coeff : -coeff;
    entry.lp_value = 0.0;
    entry.bound_diff = integer_lp_[row].ub - integer_lp_[row].lb;
    entry.expr_vars[0] =
        first_slack + 2 * IntegerVariable(tmp_slack_rows_.size());
    entry.expr_coeffs[1] = 0;
    const double activity = scaler_.UnscaleConstraintActivity(
        row, simplex_.GetConstraintActivity(row));
    if (coeff > 0) {
      // Slack = ub - constraint;
      entry.lp_value = ToDouble(integer_lp_[row].ub) - activity;
      entry.expr_coeffs[0] = IntegerValue(-1);
      entry.expr_offset = integer_lp_[row].ub;
    } else {
      // Slack = constraint - lb;
      entry.lp_value = activity - ToDouble(integer_lp_[row].lb);
      entry.expr_coeffs[0] = IntegerValue(1);
      entry.expr_offset = -integer_lp_[row].lb;
    }

    base_ct_.terms.push_back(entry);
    tmp_slack_rows_.push_back(row);
  }

  // This also make all coefficients positive.
  if (!PreprocessCut(first_slack, &base_ct_)) return false;

  // We cannot cut sum Bool <= 1.
  if (base_ct_.rhs == 1) return false;

  // Constraint with slack should be tight.
  if (DEBUG_MODE) {
    double activity = 0.0;
    double norm = 0.0;
    for (const CutTerm& term : base_ct_.terms) {
      const double coeff = ToDouble(term.coeff);
      activity += term.lp_value * coeff;
      norm += coeff * coeff;
    }
    if (std::abs(activity - static_cast<double>(base_ct_.rhs)) / norm > 1e-4) {
      VLOG(1) << "Cut not tight " << activity
              << " <= " << static_cast<double>(base_ct_.rhs);
      return false;
    }
  }

  bool at_least_one_added = false;
  DCHECK(base_ct_.AllCoefficientsArePositive());

  // Try RLT cuts.
  //
  // TODO(user): try this for more than just "base" constraints?
  if (integer_multipliers.size() == 1 && parameters_.add_rlt_cuts()) {
    if (rlt_cut_helper_.TrySimpleSeparation(base_ct_)) {
      at_least_one_added |= PostprocessAndAddCut(
          absl::StrCat(name, "_RLT"), rlt_cut_helper_.Info(), first_slack,
          rlt_cut_helper_.cut());
    }
  }

  // Note that the indexing will survive ComplementForSmallerLpValues() below.
  if (ib_processor != nullptr) {
    if (!ib_processor->CacheDataForCut(first_slack, &base_ct_)) {
      ib_processor = nullptr;
    }
  }

  // Try cover approach to find cut.
  // TODO(user): Share common computation between kinds.
  {
    cover_cut_helper_.ClearCache();

    if (cover_cut_helper_.TrySingleNodeFlow(base_ct_, ib_processor)) {
      at_least_one_added |= PostprocessAndAddCut(
          absl::StrCat(name, "_FF"), cover_cut_helper_.Info(), first_slack,
          cover_cut_helper_.cut());
    }
    if (cover_cut_helper_.TrySimpleKnapsack(base_ct_, ib_processor)) {
      at_least_one_added |= PostprocessAndAddCut(
          absl::StrCat(name, "_K"), cover_cut_helper_.Info(), first_slack,
          cover_cut_helper_.cut());
    }

    // This one need to be called after TrySimpleKnapsack() in order to reuse
    // some cached data if possible.
    if (cover_cut_helper_.TryWithLetchfordSouliLifting(base_ct_,
                                                       ib_processor)) {
      at_least_one_added |= PostprocessAndAddCut(
          absl::StrCat(name, "_KL"), cover_cut_helper_.Info(), first_slack,
          cover_cut_helper_.cut());
    }
  }

  // Try integer rounding heuristic to find cut.
  {
    base_ct_.ComplementForSmallerLpValues();

    RoundingOptions options;
    options.max_scaling = parameters_.max_integer_rounding_scaling();
    options.use_ib_before_heuristic = false;
    if (integer_rounding_cut_helper_.ComputeCut(options, base_ct_,
                                                ib_processor)) {
      at_least_one_added |= PostprocessAndAddCut(
          absl::StrCat(name, "_R"), integer_rounding_cut_helper_.Info(),
          first_slack, integer_rounding_cut_helper_.cut());
    }

    options.use_ib_before_heuristic = true;
    options.prefer_positive_ib = false;
    if (ib_processor != nullptr && integer_rounding_cut_helper_.ComputeCut(
                                       options, base_ct_, ib_processor)) {
      at_least_one_added |= PostprocessAndAddCut(
          absl::StrCat(name, "_RB"), integer_rounding_cut_helper_.Info(),
          first_slack, integer_rounding_cut_helper_.cut());
    }

    options.use_ib_before_heuristic = true;
    options.prefer_positive_ib = true;
    if (ib_processor != nullptr && integer_rounding_cut_helper_.ComputeCut(
                                       options, base_ct_, ib_processor)) {
      at_least_one_added |= PostprocessAndAddCut(
          absl::StrCat(name, "_RBP"), integer_rounding_cut_helper_.Info(),
          first_slack, integer_rounding_cut_helper_.cut());
    }
  }

  return at_least_one_added;
}

bool LinearProgrammingConstraint::PostprocessAndAddCut(
    const std::string& name, const std::string& info,
    IntegerVariable first_slack, const CutData& cut) {
  if (cut.rhs > absl::int128(std::numeric_limits<int64_t>::max()) ||
      cut.rhs < absl::int128(std::numeric_limits<int64_t>::min())) {
    VLOG(2) << "RHS overflow " << name << " " << info;
    ++num_cut_overflows_;
    return false;
  }

  // We test this here to avoid doing the costly conversion below.
  //
  // TODO(user): Ideally we should detect this even earlier during the cut
  // generation.
  if (cut.ComputeViolation() < 1e-4) {
    VLOG(2) << "Bad cut " << name << " " << info;
    ++num_bad_cuts_;
    return false;
  }

  // Substitute any slack left.
  tmp_scattered_vector_.ClearAndResize(integer_variables_.size());
  IntegerValue cut_ub = static_cast<int64_t>(cut.rhs);
  for (const CutTerm& term : cut.terms) {
    if (term.coeff == 0) continue;
    if (!AddProductTo(-term.coeff, term.expr_offset, &cut_ub)) {
      VLOG(2) << "Overflow in conversion";
      ++num_cut_overflows_;
      return false;
    }
    for (int i = 0; i < 2; ++i) {
      if (term.expr_coeffs[i] == 0) continue;
      const IntegerVariable var = term.expr_vars[i];
      const IntegerValue coeff =
          CapProd(term.coeff.value(), term.expr_coeffs[i].value());
      if (AtMinOrMaxInt64(coeff.value())) {
        VLOG(2) << "Overflow in conversion";
        ++num_cut_overflows_;
        return false;
      }

      // Simple copy for non-slack variables.
      if (var < first_slack) {
        const glop::ColIndex col =
            mirror_lp_variable_.at(PositiveVariable(var));
        if (VariableIsPositive(var)) {
          tmp_scattered_vector_.Add(col, coeff);
        } else {
          tmp_scattered_vector_.Add(col, -coeff);
        }
        continue;
      } else {
        // Replace slack from LP constraints.
        const int slack_index = (var.value() - first_slack.value()) / 2;
        const glop::RowIndex row = tmp_slack_rows_[slack_index];
        if (!tmp_scattered_vector_.AddLinearExpressionMultiple(
                coeff, IntegerLpRowCols(row), IntegerLpRowCoeffs(row))) {
          VLOG(2) << "Overflow in slack removal";
          ++num_cut_overflows_;
          return false;
        }
      }
    }
  }

  // TODO(user): avoid allocating memory if it turns out this is a duplicate of
  // something we already added. This tends to happen if the duplicate was
  // already a generated cut which is currently not part of the LP.
  LinearConstraint converted_cut =
      tmp_scattered_vector_.ConvertToLinearConstraint(integer_variables_,
                                                      cut_ub);

  // TODO(user): We probably generate too many cuts, keep best one only?
  // Note that we do need duplicate removal and maybe some orthogonality here?
  if (/*DISABLES CODE*/ (false)) {
    top_n_cuts_.AddCut(std::move(converted_cut), name, expanded_lp_solution_);
    return true;
  }
  return constraint_manager_.AddCut(std::move(converted_cut), name, info);
}

// TODO(user): This can be still too slow on some problems like
// 30_70_45_05_100.mps.gz. Not this actual function, but the set of computation
// it triggers. We should add heuristics to abort earlier if a cut is not
// promising. Or only test a few positions and not all rows.
void LinearProgrammingConstraint::AddCGCuts() {
  // We used not to do "classical" gomory and instead used this heuristic.
  // It is usually faster but on some problem like neos*creuse, this do not find
  // good cut though.
  //
  // TODO(user): Make the cut generation lighter and try this at false.
  const bool old_gomory = true;

  // Note that the index is permuted and do not correspond to a row.
  const RowIndex num_rows = lp_data_.num_constraints();
  for (RowIndex index(0); index < num_rows; ++index) {
    if (time_limit_->LimitReached()) break;

    const ColIndex basis_col = simplex_.GetBasis(index);
    const Fractional lp_value = GetVariableValueAtCpScale(basis_col);

    // Only consider fractional basis element. We ignore element that are close
    // to an integer to reduce the amount of positions we try.
    //
    // TODO(user): We could just look at the diff with std::floor() in the hope
    // that when we are just under an integer, the exact computation below will
    // also be just under it.
    if (std::abs(lp_value - std::round(lp_value)) < 0.01) continue;

    // If this variable is a slack, we ignore it. This is because the
    // corresponding row is not tight under the given lp values.
    if (old_gomory && basis_col >= integer_variables_.size()) continue;

    // TODO(user): Avoid code duplication between the sparse/dense path.
    tmp_lp_multipliers_.clear();
    const glop::ScatteredRow& lambda = simplex_.GetUnitRowLeftInverse(index);
    if (lambda.non_zeros.empty()) {
      for (RowIndex row(0); row < num_rows; ++row) {
        const double value = lambda.values[glop::RowToColIndex(row)];
        if (std::abs(value) < kZeroTolerance) continue;
        tmp_lp_multipliers_.push_back({row, value});
      }
    } else {
      for (const ColIndex col : lambda.non_zeros) {
        const RowIndex row = glop::ColToRowIndex(col);
        const double value = lambda.values[col];
        if (std::abs(value) < kZeroTolerance) continue;
        tmp_lp_multipliers_.push_back({row, value});
      }
    }

    // Size 1 is already done with MIR.
    if (tmp_lp_multipliers_.size() <= 1) continue;

    IntegerValue scaling;
    for (int i = 0; i < 2; ++i) {
      if (i == 1) {
        // Try other sign.
        //
        // TODO(user): Maybe add an heuristic to know beforehand which sign to
        // use?
        for (std::pair<RowIndex, double>& p : tmp_lp_multipliers_) {
          p.second = -p.second;
        }
      }

      // TODO(user): We use a lower value here otherwise we might run into
      // overflow while computing the cut. This should be fixable.
      tmp_integer_multipliers_ = ScaleLpMultiplier(
          /*take_objective_into_account=*/false,
          /*ignore_trivial_constraints=*/old_gomory, tmp_lp_multipliers_,
          &scaling);
      if (scaling != 0) {
        AddCutFromConstraints("CG", tmp_integer_multipliers_);
      }
    }
  }
}

// Because we know the objective is integer, the constraint objective >= lb can
// sometime cut the current lp optimal, and it can make a big difference to add
// it. Or at least use it when constructing more advanced cuts. See
// 'multisetcover_batch_0_case_115_instance_0_small_subset_elements_3_sumreqs
//  _1295_candidates_41.fzn'
//
// TODO(user): It might be better to just integrate this with the MIR code so
// that we not only consider MIR1 involving the objective but we also consider
// combining it with other constraints.
void LinearProgrammingConstraint::AddObjectiveCut() {
  if (integer_objective_.size() <= 1) return;

  // We only try to add such cut if the LB objective is "far" from the current
  // objective lower bound. Note that this is in term of the "internal" integer
  // objective.
  const double obj_lp_value = simplex_.GetObjectiveValue();
  const IntegerValue obj_lower_bound =
      integer_trail_->LevelZeroLowerBound(objective_cp_);
  if (obj_lp_value + 1.0 >= ToDouble(obj_lower_bound)) return;

  // We negate everything to have a <= base constraint.
  LinearConstraint objective_ct;
  objective_ct.lb = kMinIntegerValue;
  objective_ct.ub = integer_objective_offset_ -
                    integer_trail_->LevelZeroLowerBound(objective_cp_);
  IntegerValue obj_coeff_magnitude(0);
  objective_ct.resize(integer_objective_.size());
  int i = 0;
  for (const auto& [col, coeff] : integer_objective_) {
    const IntegerVariable var = integer_variables_[col.value()];
    objective_ct.vars[i] = var;
    objective_ct.coeffs[i] = -coeff;
    obj_coeff_magnitude = std::max(obj_coeff_magnitude, IntTypeAbs(coeff));
    ++i;
  }

  if (!base_ct_.FillFromLinearConstraint(objective_ct, expanded_lp_solution_,
                                         integer_trail_)) {
    return;
  }

  // If the magnitude is small enough, just try to add the full objective. Other
  // cuts will be derived in subsequent passes. Otherwise, try normal cut
  // heuristic that should result in a cut with reasonable coefficients.
  if (obj_coeff_magnitude < 1e9 &&
      constraint_manager_.AddCut(std::move(objective_ct), "Objective")) {
    return;
  }

  // If there are no integer (all Booleans), no need to try implied bounds
  // heurititics. By setting this to nullptr, we are a bit faster.
  ImpliedBoundsProcessor* ib_processor = nullptr;
  {
    bool some_ints = false;
    bool some_relevant_positions = false;
    for (const CutTerm& term : base_ct_.terms) {
      if (term.bound_diff > 1) some_ints = true;
      if (term.HasRelevantLpValue()) some_relevant_positions = true;
    }

    // If all value are integer, we will not be able to cut anything.
    if (!some_relevant_positions) return;
    if (some_ints) ib_processor = &implied_bounds_processor_;
  }

  // Note that the indexing will survive the complement of terms below.
  const IntegerVariable first_slack(
      std::numeric_limits<IntegerVariable::ValueType>::max());
  if (ib_processor != nullptr) {
    if (!ib_processor->CacheDataForCut(first_slack, &base_ct_)) {
      ib_processor = nullptr;
    }
  }

  // Try knapsack.
  base_ct_.ComplementForPositiveCoefficients();
  cover_cut_helper_.ClearCache();
  if (cover_cut_helper_.TrySimpleKnapsack(base_ct_, ib_processor)) {
    PostprocessAndAddCut("Objective_K", cover_cut_helper_.Info(), first_slack,
                         cover_cut_helper_.cut());
  }

  // Try rounding.
  RoundingOptions options;
  options.max_scaling = parameters_.max_integer_rounding_scaling();
  base_ct_.ComplementForSmallerLpValues();
  if (integer_rounding_cut_helper_.ComputeCut(options, base_ct_,
                                              ib_processor)) {
    PostprocessAndAddCut("Objective_R", integer_rounding_cut_helper_.Info(),
                         first_slack, integer_rounding_cut_helper_.cut());
  }
}

void LinearProgrammingConstraint::AddMirCuts() {
  // Heuristic to generate MIR_n cuts by combining a small number of rows. This
  // works greedily and follow more or less the MIR cut description in the
  // literature. We have a current cut, and we add one more row to it while
  // eliminating a variable of the current cut whose LP value is far from its
  // bound.
  //
  // A notable difference is that we randomize the variable we eliminate and
  // the row we use to do so. We still have weights to indicate our preferred
  // choices. This allows to generate different cuts when called again and
  // again.
  //
  // TODO(user): We could combine n rows to make sure we eliminate n variables
  // far away from their bounds by solving exactly in integer small linear
  // system.
  util_intops::StrongVector<ColIndex, IntegerValue> dense_cut(
      integer_variables_.size(), IntegerValue(0));
  SparseBitset<ColIndex> non_zeros_(ColIndex(integer_variables_.size()));

  // We compute all the rows that are tight, these will be used as the base row
  // for the MIR_n procedure below.
  const int num_rows = lp_data_.num_constraints().value();
  std::vector<std::pair<RowIndex, IntegerValue>> base_rows;
  util_intops::StrongVector<RowIndex, double> row_weights(num_rows, 0.0);
  util_intops::StrongVector<RowIndex, bool> at_ub(num_rows, false);
  util_intops::StrongVector<RowIndex, bool> at_lb(num_rows, false);
  for (RowIndex row(0); row < num_rows; ++row) {
    // We only consider tight rows.
    // We use both the status and activity to have as much options as possible.
    //
    // TODO(user): shall we consider rows that are not tight?
    // TODO(user): Ignore trivial rows? Note that we do that for MIR1 since it
    // cannot be good.
    const auto status = simplex_.GetConstraintStatus(row);
    const double activity = simplex_.GetConstraintActivity(row);
    if (activity > lp_data_.constraint_upper_bounds()[row] - 1e-4 ||
        status == glop::ConstraintStatus::AT_UPPER_BOUND ||
        status == glop::ConstraintStatus::FIXED_VALUE) {
      at_ub[row] = true;
      base_rows.push_back({row, IntegerValue(1)});
    }
    if (activity < lp_data_.constraint_lower_bounds()[row] + 1e-4 ||
        status == glop::ConstraintStatus::AT_LOWER_BOUND ||
        status == glop::ConstraintStatus::FIXED_VALUE) {
      at_lb[row] = true;
      base_rows.push_back({row, IntegerValue(-1)});
    }

    // For now, we use the dual values for the row "weights".
    //
    // Note that we use the dual at LP scale so that it make more sense when we
    // compare different rows since the LP has been scaled.
    //
    // TODO(user): In Kati Wolter PhD "Implementation of Cutting Plane
    // Separators for Mixed Integer Programs" which describe SCIP's MIR cuts
    // implementation (or at least an early version of it), a more complex score
    // is used.
    //
    // Note(user): Because we only consider tight rows under the current lp
    // solution (i.e. non-basic rows), most should have a non-zero dual values.
    // But there is some degenerate problem where these rows have a really low
    // weight (or even zero), and having only weight of exactly zero in
    // std::discrete_distribution will result in a crash.
    row_weights[row] = std::max(1e-8, std::abs(simplex_.GetDualValue(row)));
  }

  // The code here can be really slow, so we put a limit on the number of
  // entries we process. We randomize the base_rows so that on the next calls
  // we do not do exactly the same if we can't process many base row.
  int64_t dtime_num_entries = 0;
  std::shuffle(base_rows.begin(), base_rows.end(), *random_);

  std::vector<double> weights;
  util_intops::StrongVector<RowIndex, bool> used_rows;
  std::vector<std::pair<RowIndex, IntegerValue>> integer_multipliers;
  for (const std::pair<RowIndex, IntegerValue>& entry : base_rows) {
    if (time_limit_->LimitReached()) break;
    if (dtime_num_entries > 1e7) break;

    const glop::RowIndex base_row = entry.first;
    const IntegerValue multiplier = entry.second;

    // First try to generate a cut directly from this base row (MIR1).
    //
    // Note(user): We abort on success like it seems to be done in the
    // literature. Note that we don't succeed that often in generating an
    // efficient cut, so I am not sure aborting will make a big difference
    // speedwise. We might generate similar cuts though, but hopefully the cut
    // management can deal with that.
    integer_multipliers = {entry};
    if ((multiplier > 0 && !integer_lp_[base_row].ub_is_trivial) ||
        (multiplier < 0 && !integer_lp_[base_row].lb_is_trivial)) {
      if (AddCutFromConstraints("MIR_1", integer_multipliers)) continue;
    }

    // Cleanup.
    for (const ColIndex col : non_zeros_.PositionsSetAtLeastOnce()) {
      dense_cut[col] = IntegerValue(0);
    }
    non_zeros_.SparseClearAll();

    // Copy cut.
    const LinearConstraintInternal& ct = integer_lp_[entry.first];
    for (int i = 0; i < ct.num_terms; ++i) {
      const int index = ct.start_in_buffer + i;
      const ColIndex col = integer_lp_cols_[index];
      const IntegerValue coeff = integer_lp_coeffs_[index];
      non_zeros_.Set(col);
      dense_cut[col] += coeff * multiplier;
    }

    used_rows.assign(num_rows, false);
    used_rows[entry.first] = true;

    // We will aggregate at most kMaxAggregation more rows.
    //
    // TODO(user): optim + tune.
    const int kMaxAggregation = 5;
    for (int i = 0; i < kMaxAggregation; ++i) {
      // First pick a variable to eliminate. We currently pick a random one with
      // a weight that depend on how far it is from its closest bound.
      IntegerValue max_magnitude(0);
      weights.clear();
      std::vector<ColIndex> col_candidates;
      for (const ColIndex col : non_zeros_.PositionsSetAtLeastOnce()) {
        if (dense_cut[col] == 0) continue;

        max_magnitude = std::max(max_magnitude, IntTypeAbs(dense_cut[col]));
        const int col_degree =
            lp_data_.GetSparseColumn(col).num_entries().value();
        if (col_degree <= 1) continue;
        if (simplex_.GetVariableStatus(col) != glop::VariableStatus::BASIC) {
          continue;
        }

        const IntegerVariable var = integer_variables_[col.value()];
        const double lp_value = expanded_lp_solution_[var];
        const double lb = ToDouble(integer_trail_->LevelZeroLowerBound(var));
        const double ub = ToDouble(integer_trail_->LevelZeroUpperBound(var));
        const double bound_distance = std::min(ub - lp_value, lp_value - lb);
        if (bound_distance > 1e-2) {
          weights.push_back(bound_distance);
          col_candidates.push_back(col);
        }
      }
      if (col_candidates.empty()) break;

      const ColIndex var_to_eliminate =
          col_candidates[WeightedPick(weights, *random_)];

      // What rows can we add to eliminate var_to_eliminate?
      std::vector<RowIndex> possible_rows;
      weights.clear();
      for (const auto entry : lp_data_.GetSparseColumn(var_to_eliminate)) {
        const RowIndex row = entry.row();

        // We disallow all the rows that contain a variable that we already
        // eliminated (or are about to). This mean that we choose rows that
        // form a "triangular" matrix on the position we choose to eliminate.
        if (used_rows[row]) continue;
        used_rows[row] = true;

        // We only consider "tight" rows, as defined above.
        bool add_row = false;
        if (at_ub[row]) {
          if (entry.coefficient() > 0.0) {
            if (dense_cut[var_to_eliminate] < 0) add_row = true;
          } else {
            if (dense_cut[var_to_eliminate] > 0) add_row = true;
          }
        }
        if (at_lb[row]) {
          if (entry.coefficient() > 0.0) {
            if (dense_cut[var_to_eliminate] > 0) add_row = true;
          } else {
            if (dense_cut[var_to_eliminate] < 0) add_row = true;
          }
        }
        if (add_row) {
          possible_rows.push_back(row);
          weights.push_back(row_weights[row]);
        }
      }
      if (possible_rows.empty()) break;

      const RowIndex row_to_combine =
          possible_rows[WeightedPick(weights, *random_)];

      // Find the coefficient of the variable to eliminate.
      IntegerValue to_combine_coeff = 0;
      const LinearConstraintInternal& ct_to_combine =
          integer_lp_[row_to_combine];
      for (int i = 0; i < ct_to_combine.num_terms; ++i) {
        const int index = ct_to_combine.start_in_buffer + i;
        if (integer_lp_cols_[index] == var_to_eliminate) {
          to_combine_coeff = integer_lp_coeffs_[index];
          break;
        }
      }
      CHECK_NE(to_combine_coeff, 0);

      IntegerValue mult1 = -to_combine_coeff;
      IntegerValue mult2 = dense_cut[var_to_eliminate];
      CHECK_NE(mult2, 0);
      if (mult1 < 0) {
        mult1 = -mult1;
        mult2 = -mult2;
      }

      const IntegerValue gcd = IntegerValue(
          MathUtil::GCD64(std::abs(mult1.value()), std::abs(mult2.value())));
      CHECK_NE(gcd, 0);
      mult1 /= gcd;
      mult2 /= gcd;

      // Overflow detection.
      //
      // TODO(user): do that in the possible_rows selection? only problem is
      // that we do not have the integer coefficient there...
      for (std::pair<RowIndex, IntegerValue>& entry : integer_multipliers) {
        max_magnitude = std::max(max_magnitude, IntTypeAbs(entry.second));
      }
      if (CapAdd(CapProd(max_magnitude.value(), std::abs(mult1.value())),
                 CapProd(infinity_norms_[row_to_combine].value(),
                         std::abs(mult2.value()))) ==
          std::numeric_limits<int64_t>::max()) {
        break;
      }

      for (std::pair<RowIndex, IntegerValue>& entry : integer_multipliers) {
        dtime_num_entries += integer_lp_[entry.first].num_terms;
        entry.second *= mult1;
      }
      dtime_num_entries += integer_lp_[row_to_combine].num_terms;
      integer_multipliers.push_back({row_to_combine, mult2});

      // TODO(user): Not supper efficient to recombine the rows.
      if (AddCutFromConstraints(absl::StrCat("MIR_", i + 2),
                                integer_multipliers)) {
        break;
      }

      // Minor optim: the computation below is only needed if we do one more
      // iteration.
      if (i + 1 == kMaxAggregation) break;

      for (ColIndex col : non_zeros_.PositionsSetAtLeastOnce()) {
        dense_cut[col] *= mult1;
      }
      for (int i = 0; i < ct_to_combine.num_terms; ++i) {
        const int index = ct_to_combine.start_in_buffer + i;
        const ColIndex col = integer_lp_cols_[index];
        const IntegerValue coeff = integer_lp_coeffs_[index];
        non_zeros_.Set(col);
        dense_cut[col] += coeff * mult2;
      }
    }
  }
}

void LinearProgrammingConstraint::AddZeroHalfCuts() {
  if (time_limit_->LimitReached()) return;

  tmp_lp_values_.clear();
  tmp_var_lbs_.clear();
  tmp_var_ubs_.clear();
  for (const IntegerVariable var : integer_variables_) {
    tmp_lp_values_.push_back(expanded_lp_solution_[var]);
    tmp_var_lbs_.push_back(integer_trail_->LevelZeroLowerBound(var));
    tmp_var_ubs_.push_back(integer_trail_->LevelZeroUpperBound(var));
  }

  // TODO(user): See if it make sense to try to use implied bounds there.
  zero_half_cut_helper_.ProcessVariables(tmp_lp_values_, tmp_var_lbs_,
                                         tmp_var_ubs_);
  for (glop::RowIndex row(0); row < integer_lp_.size(); ++row) {
    // Even though we could use non-tight row, for now we prefer to use tight
    // ones.
    const auto status = simplex_.GetConstraintStatus(row);
    if (status == glop::ConstraintStatus::BASIC) continue;
    if (status == glop::ConstraintStatus::FREE) continue;

    zero_half_cut_helper_.AddOneConstraint(
        row, IntegerLpRowCols(row), IntegerLpRowCoeffs(row),
        integer_lp_[row].lb, integer_lp_[row].ub);
  }
  for (const std::vector<std::pair<RowIndex, IntegerValue>>& multipliers :
       zero_half_cut_helper_.InterestingCandidates(random_)) {
    if (time_limit_->LimitReached()) break;

    // TODO(user): Make sure that if the resulting linear coefficients are not
    // too high, we do try a "divisor" of two and thus try a true zero-half cut
    // instead of just using our best MIR heuristic (which might still be better
    // though).
    AddCutFromConstraints("ZERO_HALF", multipliers);
  }
}

void LinearProgrammingConstraint::UpdateSimplexIterationLimit(
    const int64_t min_iter, const int64_t max_iter) {
  if (parameters_.linearization_level() < 2) return;
  const int64_t num_degenerate_columns = CalculateDegeneracy();
  const int64_t num_cols = simplex_.GetProblemNumCols().value();
  if (num_cols <= 0) {
    return;
  }
  CHECK_GT(num_cols, 0);
  const int64_t decrease_factor = (10 * num_degenerate_columns) / num_cols;
  if (simplex_.GetProblemStatus() == glop::ProblemStatus::DUAL_FEASIBLE) {
    // We reached here probably because we predicted wrong. We use this as a
    // signal to increase the iterations or punish less for degeneracy compare
    // to the other part.
    if (is_degenerate_) {
      next_simplex_iter_ /= std::max(int64_t{1}, decrease_factor);
    } else {
      next_simplex_iter_ *= 2;
    }
  } else if (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL) {
    if (is_degenerate_) {
      next_simplex_iter_ /= std::max(int64_t{1}, 2 * decrease_factor);
    } else {
      // This is the most common case. We use the size of the problem to
      // determine the limit and ignore the previous limit.
      next_simplex_iter_ = num_cols / 40;
    }
  }
  next_simplex_iter_ =
      std::max(min_iter, std::min(max_iter, next_simplex_iter_));
}

bool LinearProgrammingConstraint::Propagate() {
  if (!enabled_) return true;
  if (time_limit_->LimitReached()) return true;
  UpdateBoundsOfLpVariables();

  // TODO(user): It seems the time we loose by not stopping early might be worth
  // it because we end up with a better explanation at optimality.
  if (/* DISABLES CODE */ (false) && objective_is_defined_) {
    // We put a limit on the dual objective since there is no point increasing
    // it past our current objective upper-bound (we will already fail as soon
    // as we pass it). Note that this limit is properly transformed using the
    // objective scaling factor and offset stored in lp_data_.
    //
    // Note that we use a bigger epsilon here to be sure that if we abort
    // because of this, we will report a conflict.
    simplex_params_.set_objective_upper_limit(
        static_cast<double>(integer_trail_->UpperBound(objective_cp_).value() +
                            100.0 * kCpEpsilon));
  }

  // Put an iteration limit on the work we do in the simplex for this call. Note
  // that because we are "incremental", even if we don't solve it this time we
  // will make progress towards a solve in the lower node of the tree search.
  if (trail_->CurrentDecisionLevel() == 0) {
    simplex_params_.set_max_number_of_iterations(
        parameters_.root_lp_iterations());
  } else {
    simplex_params_.set_max_number_of_iterations(next_simplex_iter_);
  }

  simplex_.SetParameters(simplex_params_);
  if (!SolveLp()) return true;
  if (!AnalyzeLp()) return false;

  // Add new constraints to the LP and resolve?
  const int max_cuts_rounds = trail_->CurrentDecisionLevel() == 0
                                  ? parameters_.max_cut_rounds_at_level_zero()
                                  : 1;
  int cuts_round = 0;
  while (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL &&
         cuts_round < max_cuts_rounds) {
    // We wait for the first batch of problem constraints to be added before we
    // begin to generate cuts. Note that we rely on num_solves_ since on some
    // problems there is no other constraints than the cuts.
    cuts_round++;
    if (parameters_.cut_level() > 0 &&
        (num_solves_ > 1 || !parameters_.add_lp_constraints_lazily())) {
      // This must be called first.
      implied_bounds_processor_.RecomputeCacheAndSeparateSomeImpliedBoundCuts(
          expanded_lp_solution_);
      if (parameters_.add_rlt_cuts()) {
        rlt_cut_helper_.Initialize(mirror_lp_variable_);
      }

      // The "generic" cuts are currently part of this class as they are using
      // data from the current LP.
      //
      // TODO(user): Refactor so that they are just normal cut generators?
      const int level = trail_->CurrentDecisionLevel();
      if (trail_->CurrentDecisionLevel() == 0) {
        problem_proven_infeasible_by_cuts_ = false;
        if (parameters_.add_objective_cut()) AddObjectiveCut();
        if (parameters_.add_mir_cuts()) AddMirCuts();
        if (parameters_.add_cg_cuts()) AddCGCuts();
        if (parameters_.add_zero_half_cuts()) AddZeroHalfCuts();
        if (problem_proven_infeasible_by_cuts_) {
          return integer_trail_->ReportConflict({});
        }
        top_n_cuts_.TransferToManager(&constraint_manager_);
      }

      // Try to add cuts.
      if (level == 0 || !parameters_.only_add_cuts_at_level_zero()) {
        for (const CutGenerator& generator : cut_generators_) {
          if (level > 0 && generator.only_run_at_level_zero) continue;
          if (!generator.generate_cuts(&constraint_manager_)) {
            return false;
          }
        }
      }

      implied_bounds_processor_.IbCutPool().TransferToManager(
          &constraint_manager_);
    }

    int num_added = 0;
    if (constraint_manager_.ChangeLp(&state_, &num_added)) {
      ++num_lp_changes_;
      simplex_.LoadStateForNextSolve(state_);
      if (!CreateLpFromConstraintManager()) {
        return integer_trail_->ReportConflict({});
      }

      // If we didn't add any new constraint, we delay the next Solve() since
      // likely the optimal didn't change.
      if (num_added == 0) {
        break;
      }

      const double old_obj = simplex_.GetObjectiveValue();
      if (!SolveLp()) return true;
      if (!AnalyzeLp()) return false;
      if (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL) {
        VLOG(3) << "Relaxation improvement " << old_obj << " -> "
                << simplex_.GetObjectiveValue()
                << " diff: " << simplex_.GetObjectiveValue() - old_obj
                << " level: " << trail_->CurrentDecisionLevel();
      }
    } else {
      if (trail_->CurrentDecisionLevel() == 0) {
        lp_at_level_zero_is_final_ = true;
      }
      break;
    }
  }

  return true;
}

absl::int128 LinearProgrammingConstraint::GetImpliedLowerBound(
    const LinearConstraint& terms) const {
  absl::int128 lower_bound(0);
  const int size = terms.num_terms;
  for (int i = 0; i < size; ++i) {
    const IntegerVariable var = terms.vars[i];
    const IntegerValue coeff = terms.coeffs[i];
    const IntegerValue bound = coeff > 0 ? integer_trail_->LowerBound(var)
                                         : integer_trail_->UpperBound(var);
    lower_bound += absl::int128(bound.value()) * absl::int128(coeff.value());
  }
  return lower_bound;
}

bool LinearProgrammingConstraint::ScalingCanOverflow(
    int power, bool take_objective_into_account,
    absl::Span<const std::pair<glop::RowIndex, double>> multipliers,
    int64_t overflow_cap) const {
  int64_t bound = 0;
  const int64_t factor = int64_t{1} << power;
  const double factor_as_double = static_cast<double>(factor);
  if (take_objective_into_account) {
    bound = CapAdd(bound, CapProd(factor, objective_infinity_norm_.value()));
    if (bound >= overflow_cap) return true;
  }
  for (const auto [row, double_coeff] : multipliers) {
    const double magnitude =
        std::abs(std::round(double_coeff * factor_as_double));
    if (std::isnan(magnitude)) return true;
    if (magnitude >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
      return true;
    }
    bound = CapAdd(bound, CapProd(static_cast<int64_t>(magnitude),
                                  infinity_norms_[row].value()));
    if (bound >= overflow_cap) return true;
  }
  return bound >= overflow_cap;
}

std::vector<std::pair<RowIndex, IntegerValue>>
LinearProgrammingConstraint::ScaleLpMultiplier(
    bool take_objective_into_account, bool ignore_trivial_constraints,
    const std::vector<std::pair<RowIndex, double>>& lp_multipliers,
    IntegerValue* scaling, int64_t overflow_cap) const {
  *scaling = 0;

  // First unscale the values with the LP scaling and remove bad cases.
  tmp_cp_multipliers_.clear();
  for (const std::pair<RowIndex, double>& p : lp_multipliers) {
    const RowIndex row = p.first;
    const Fractional lp_multi = p.second;

    // We ignore small values since these are likely errors and will not
    // contribute much to the new lp constraint anyway.
    if (std::abs(lp_multi) < kZeroTolerance) continue;

    // Remove constraints that shouldn't be helpful.
    //
    // In practice, because we can complement the slack, it might still be
    // useful to have some constraint with a trivial upper bound.
    if (ignore_trivial_constraints) {
      if (lp_multi > 0.0 && integer_lp_[row].ub_is_trivial) {
        continue;
      }
      if (lp_multi < 0.0 && integer_lp_[row].lb_is_trivial) {
        continue;
      }
    }

    tmp_cp_multipliers_.push_back(
        {row, scaler_.UnscaleDualValue(row, lp_multi)});
  }

  std::vector<std::pair<RowIndex, IntegerValue>> integer_multipliers;
  if (tmp_cp_multipliers_.empty()) {
    // Empty linear combinaison.
    return integer_multipliers;
  }

  // TODO(user): we currently do not support scaling down, so we just abort
  // if with a scaling of 1, we reach the overflow_cap.
  if (ScalingCanOverflow(/*power=*/0, take_objective_into_account,
                         tmp_cp_multipliers_, overflow_cap)) {
    ++num_scaling_issues_;
    return integer_multipliers;
  }

  // Note that we don't try to scale by more than 63 since in practice the
  // constraint multipliers should not be all super small.
  //
  // TODO(user): We could be faster here, but trying to compute the exact power
  // in one go with floating points seems tricky. So we just do around 6 passes
  // plus the one above for zero.
  const int power = BinarySearch<int>(0, 63, [&](int candidate) {
    // Because BinarySearch() wants f(upper_bound) to fail, we bypass the test
    // here as when the set of floating points are really small, we could pass
    // with a large power.
    if (candidate >= 63) return false;

    return !ScalingCanOverflow(candidate, take_objective_into_account,
                               tmp_cp_multipliers_, overflow_cap);
  });
  *scaling = int64_t{1} << power;

  // Scale the multipliers by *scaling.
  // Note that we use the exact same formula as in ScalingCanOverflow().
  int64_t gcd = scaling->value();
  const double scaling_as_double = static_cast<double>(scaling->value());
  for (const auto [row, double_coeff] : tmp_cp_multipliers_) {
    const IntegerValue coeff(std::round(double_coeff * scaling_as_double));
    if (coeff != 0) {
      gcd = std::gcd(gcd, std::abs(coeff.value()));
      integer_multipliers.push_back({row, coeff});
    }
  }
  if (gcd > 1) {
    *scaling /= gcd;
    for (auto& entry : integer_multipliers) {
      entry.second /= gcd;
    }
  }
  return integer_multipliers;
}

template <bool check_overflow>
bool LinearProgrammingConstraint::ComputeNewLinearConstraint(
    absl::Span<const std::pair<RowIndex, IntegerValue>> integer_multipliers,
    ScatteredIntegerVector* scattered_vector, IntegerValue* upper_bound) const {
  // Initialize the new constraint.
  *upper_bound = 0;
  scattered_vector->ClearAndResize(integer_variables_.size());

  // Compute the new constraint by taking the linear combination given by
  // integer_multipliers of the integer constraints in integer_lp_.
  for (const std::pair<RowIndex, IntegerValue>& term : integer_multipliers) {
    const RowIndex row = term.first;
    const IntegerValue multiplier = term.second;
    CHECK_LT(row, integer_lp_.size());

    // Update the constraint.
    if (!scattered_vector->AddLinearExpressionMultiple<check_overflow>(
            multiplier, IntegerLpRowCols(row), IntegerLpRowCoeffs(row))) {
      return false;
    }

    // Update the upper bound.
    const IntegerValue bound =
        multiplier > 0 ? integer_lp_[row].ub : integer_lp_[row].lb;
    if (!AddProductTo(multiplier, bound, upper_bound)) return false;
  }

  return true;
}

// TODO(user): no need to update the multipliers.
void LinearProgrammingConstraint::AdjustNewLinearConstraint(
    std::vector<std::pair<glop::RowIndex, IntegerValue>>* integer_multipliers,
    ScatteredIntegerVector* scattered_vector, IntegerValue* upper_bound) const {
  const IntegerValue kMaxWantedCoeff(1e18);
  bool adjusted = false;
  for (std::pair<RowIndex, IntegerValue>& term : *integer_multipliers) {
    const RowIndex row = term.first;
    const IntegerValue multiplier = term.second;
    if (multiplier == 0) continue;

    // We will only allow change of the form "multiplier += to_add" with to_add
    // in [-negative_limit, positive_limit].
    //
    // We do not want to_add * row to overflow.
    IntegerValue negative_limit =
        FloorRatio(kMaxWantedCoeff, infinity_norms_[row]);
    IntegerValue positive_limit = negative_limit;

    // Make sure we never change the sign of the multiplier, except if the
    // row is an equality in which case we don't care.
    if (integer_lp_[row].ub != integer_lp_[row].lb) {
      if (multiplier > 0) {
        negative_limit = std::min(negative_limit, multiplier);
      } else {
        positive_limit = std::min(positive_limit, -multiplier);
      }
    }

    // Make sure upper_bound + to_add * row_bound never overflow.
    const IntegerValue row_bound =
        multiplier > 0 ? integer_lp_[row].ub : integer_lp_[row].lb;
    if (row_bound != 0) {
      const IntegerValue limit1 = FloorRatio(
          std::max(IntegerValue(0), kMaxWantedCoeff - IntTypeAbs(*upper_bound)),
          IntTypeAbs(row_bound));
      const IntegerValue limit2 =
          FloorRatio(kMaxWantedCoeff, IntTypeAbs(row_bound));
      if ((*upper_bound > 0) == (row_bound > 0)) {  // Same sign.
        positive_limit = std::min(positive_limit, limit1);
        negative_limit = std::min(negative_limit, limit2);
      } else {
        negative_limit = std::min(negative_limit, limit1);
        positive_limit = std::min(positive_limit, limit2);
      }
    }

    // If we add the row to the scattered_vector, diff will indicate by how much
    // |upper_bound - ImpliedLB(scattered_vector)| will change. That correspond
    // to increasing the multiplier by 1.
    //
    // At this stage, we are not sure computing sum coeff * bound will not
    // overflow, so we use floating point numbers. It is fine to do so since
    // this is not directly involved in the actual exact constraint generation:
    // these variables are just used in an heuristic.
    double common_diff = ToDouble(row_bound);
    double positive_diff = 0.0;
    double negative_diff = 0.0;

    // TODO(user): we could relax a bit some of the condition and allow a sign
    // change. It is just trickier to compute the diff when we allow such
    // changes.
    const LinearConstraintInternal& ct = integer_lp_[row];
    for (int i = 0; i < ct.num_terms; ++i) {
      const int index = ct.start_in_buffer + i;
      const ColIndex col = integer_lp_cols_[index];
      const IntegerValue coeff = integer_lp_coeffs_[index];
      DCHECK_NE(coeff, 0);

      // Moving a variable away from zero seems to improve the bound even
      // if it reduces the number of non-zero. Note that this is because of
      // this that positive_diff and negative_diff are not the same.
      const IntegerValue current = (*scattered_vector)[col];
      if (current == 0) {
        const IntegerVariable var = integer_variables_[col.value()];
        const IntegerValue lb = integer_trail_->LowerBound(var);
        const IntegerValue ub = integer_trail_->UpperBound(var);
        if (coeff > 0) {
          positive_diff -= ToDouble(coeff) * ToDouble(lb);
          negative_diff -= ToDouble(coeff) * ToDouble(ub);
        } else {
          positive_diff -= ToDouble(coeff) * ToDouble(ub);
          negative_diff -= ToDouble(coeff) * ToDouble(lb);
        }
        continue;
      }

      // We don't want to change the sign of current (except if the variable is
      // fixed, but in practice we should have removed any fixed variable, so we
      // don't care here) or to have an overflow.
      //
      // Corner case:
      //  - IntTypeAbs(current) can be larger than kMaxWantedCoeff!
      //  - The code assumes that 2 * kMaxWantedCoeff do not overflow.
      //
      // Note that because we now that limit * abs_coeff will never overflow
      // because we used infinity_norms_[row] above.
      const IntegerValue abs_coeff = IntTypeAbs(coeff);
      const IntegerValue current_magnitude = IntTypeAbs(current);
      const IntegerValue overflow_threshold =
          std::max(IntegerValue(0), kMaxWantedCoeff - current_magnitude);
      if ((current > 0) == (coeff > 0)) {  // Same sign.
        if (negative_limit * abs_coeff > current_magnitude) {
          negative_limit = current_magnitude / abs_coeff;
          if (positive_limit == 0 && negative_limit == 0) break;
        }
        if (positive_limit * abs_coeff > overflow_threshold) {
          positive_limit = overflow_threshold / abs_coeff;
          if (positive_limit == 0 && negative_limit == 0) break;
        }
      } else {
        if (negative_limit * abs_coeff > overflow_threshold) {
          negative_limit = overflow_threshold / abs_coeff;
          if (positive_limit == 0 && negative_limit == 0) break;
        }
        if (positive_limit * abs_coeff > current_magnitude) {
          positive_limit = current_magnitude / abs_coeff;
          if (positive_limit == 0 && negative_limit == 0) break;
        }
      }

      // This is how diff change.
      const IntegerVariable var = integer_variables_[col.value()];
      const IntegerValue implied = current > 0
                                       ? integer_trail_->LowerBound(var)
                                       : integer_trail_->UpperBound(var);
      if (implied != 0) {
        common_diff -= ToDouble(coeff) * ToDouble(implied);
      }
    }

    positive_diff += common_diff;
    negative_diff += common_diff;

    // Only add a multiple of this row if it tighten the final constraint.
    // The positive_diff/negative_diff are supposed to be integer modulo the
    // double precision, so we only add a multiple if they seems far away from
    // zero.
    IntegerValue to_add(0);
    if (positive_diff <= -1.0 && positive_limit > 0) {
      to_add = positive_limit;
    }
    if (negative_diff >= 1.0 && negative_limit > 0) {
      // Pick this if it is better than the positive sign.
      if (to_add == 0 ||
          std::abs(ToDouble(negative_limit) * negative_diff) >
              std::abs(ToDouble(positive_limit) * positive_diff)) {
        to_add = -negative_limit;
      }
    }
    if (to_add != 0) {
      term.second += to_add;
      *upper_bound += to_add * row_bound;

      // TODO(user): we could avoid checking overflow here, but this is likely
      // not in the hot loop.
      adjusted = true;
      CHECK(scattered_vector
                ->AddLinearExpressionMultiple</*check_overflow=*/false>(
                    to_add, IntegerLpRowCols(row), IntegerLpRowCoeffs(row)));
    }
  }
  if (adjusted) ++num_adjusts_;
}

bool LinearProgrammingConstraint::PropagateLpConstraint(LinearConstraint ct) {
  DCHECK(constraint_manager_.DebugCheckConstraint(ct));

  // We need to cache this before we std::move() the constraint!
  const int num_terms = ct.num_terms;
  if (num_terms == 0) {
    if (ct.ub >= 0) return true;
    return integer_trail_->ReportConflict({});  // Unsat.
  }

  std::unique_ptr<IntegerSumLE128> cp_constraint(
      new IntegerSumLE128(std::move(ct), model_));

  // We always propagate level zero bounds first.
  // If we are at level zero, there is nothing else to do.
  if (!cp_constraint->PropagateAtLevelZero()) return false;
  if (trail_->CurrentDecisionLevel() == 0) return true;

  // To optimize the memory usage, if the constraint didn't propagate anything,
  // we don't need to keep it around.
  const int64_t stamp = integer_trail_->num_enqueues();
  const bool no_conflict = cp_constraint->Propagate();
  if (no_conflict && integer_trail_->num_enqueues() == stamp) return true;

  const int64_t current_size =
      cumulative_optimal_constraint_sizes_.empty()
          ? 0
          : cumulative_optimal_constraint_sizes_.back();
  optimal_constraints_.push_back(std::move(cp_constraint));
  cumulative_optimal_constraint_sizes_.push_back(current_size + num_terms);
  rev_optimal_constraints_size_ = optimal_constraints_.size();
  return no_conflict;
}

// The "exact" computation go as follows:
//
// Given any INTEGER linear combination of the LP constraints, we can create a
// new integer constraint that is valid (its computation must not overflow
// though). Lets call this "linear_combination <= ub". We can then always add to
// it the inequality "objective_terms <= objective_var", so we get:
// ImpliedLB(objective_terms + linear_combination) - ub <= objective_var.
// where ImpliedLB() is computed from the variable current bounds.
//
// Now, if we use for the linear combination and approximation of the optimal
// negated dual LP values (by scaling them and rounding them to integer), we
// will get an EXACT objective lower bound that is more or less the same as the
// inexact bound given by the LP relaxation. This allows to derive exact reasons
// for any propagation done by this constraint.
bool LinearProgrammingConstraint::PropagateExactLpReason() {
  // Clear old reason and deductions.
  integer_reason_.clear();
  deductions_.clear();
  deductions_reason_.clear();

  // The row multipliers will be the negation of the LP duals.
  //
  // TODO(user): Provide and use a sparse API in Glop to get the duals.
  const RowIndex num_rows = simplex_.GetProblemNumRows();
  tmp_lp_multipliers_.clear();
  for (RowIndex row(0); row < num_rows; ++row) {
    const double value = -simplex_.GetDualValue(row);
    if (std::abs(value) < kZeroTolerance) continue;
    tmp_lp_multipliers_.push_back({row, value});
  }

  // In this case, the LP lower bound match the basic objective "constraint"
  // propagation. That is there is an LP solution with all objective variable at
  // their current best bound. There is no need to do more work here.
  if (tmp_lp_multipliers_.empty()) return true;

  // For the corner case of an objective of size 1, we do not want or need
  // to take it into account.
  bool take_objective_into_account = true;
  if (mirror_lp_variable_.contains(objective_cp_)) {
    // The objective is part of the lp.
    // This should only happen for objective with a single term.
    CHECK_EQ(integer_objective_.size(), 1);
    CHECK_EQ(integer_objective_[0].first,
             mirror_lp_variable_.at(objective_cp_));
    CHECK_EQ(integer_objective_[0].second, IntegerValue(1));

    take_objective_into_account = false;
  }

  IntegerValue scaling = 0;
  tmp_integer_multipliers_ = ScaleLpMultiplier(
      take_objective_into_account,
      /*ignore_trivial_constraints=*/true, tmp_lp_multipliers_, &scaling);
  if (scaling == 0) {
    VLOG(1) << simplex_.GetProblemStatus();
    VLOG(1) << "Issue while computing the exact LP reason. Aborting.";
    return true;
  }

  IntegerValue rc_ub;
  CHECK(ComputeNewLinearConstraint</*check_overflow=*/false>(
      tmp_integer_multipliers_, &tmp_scattered_vector_, &rc_ub));

  std::optional<std::pair<IntegerVariable, IntegerValue>> extra_term =
      std::nullopt;
  if (take_objective_into_account) {
    // The "objective constraint" behave like if the unscaled cp multiplier was
    // 1.0, so we will multiply it by this number and add it to reduced_costs.
    const IntegerValue obj_scale = scaling;

    // TODO(user): Maybe avoid this conversion.
    tmp_cols_.clear();
    tmp_coeffs_.clear();
    for (const auto [col, coeff] : integer_objective_) {
      tmp_cols_.push_back(col);
      tmp_coeffs_.push_back(coeff);
    }
    CHECK(tmp_scattered_vector_
              .AddLinearExpressionMultiple</*check_overflow=*/false>(
                  obj_scale, tmp_cols_, tmp_coeffs_));
    CHECK(AddProductTo(-obj_scale, integer_objective_offset_, &rc_ub));

    extra_term = {objective_cp_, -obj_scale};
  }

  AdjustNewLinearConstraint(&tmp_integer_multipliers_, &tmp_scattered_vector_,
                            &rc_ub);

  // Create the IntegerSumLE that will allow to propagate the objective and more
  // generally do the reduced cost fixing.
  LinearConstraint explanation =
      tmp_scattered_vector_.ConvertToLinearConstraint(integer_variables_, rc_ub,
                                                      extra_term);

  // Corner case where prevent overflow removed all terms.
  if (explanation.num_terms == 0) {
    trail_->MutableConflict()->clear();
    return explanation.ub >= 0;
  }

  return PropagateLpConstraint(std::move(explanation));
}

bool LinearProgrammingConstraint::PropagateExactDualRay() {
  IntegerValue scaling;
  const glop::DenseColumn ray = simplex_.GetDualRay();
  tmp_lp_multipliers_.clear();
  for (RowIndex row(0); row < ray.size(); ++row) {
    const double value = ray[row];
    if (std::abs(value) < kZeroTolerance) continue;
    tmp_lp_multipliers_.push_back({row, value});
  }
  tmp_integer_multipliers_ = ScaleLpMultiplier(
      /*take_objective_into_account=*/false,
      /*ignore_trivial_constraints=*/true, tmp_lp_multipliers_, &scaling);
  if (scaling == 0) {
    VLOG(1) << "Isse while computing the exact dual ray reason. Aborting.";
    return true;
  }

  IntegerValue new_constraint_ub;
  CHECK(ComputeNewLinearConstraint</*check_overflow=*/false>(
      tmp_integer_multipliers_, &tmp_scattered_vector_, &new_constraint_ub))
      << scaling;

  AdjustNewLinearConstraint(&tmp_integer_multipliers_, &tmp_scattered_vector_,
                            &new_constraint_ub);

  LinearConstraint explanation =
      tmp_scattered_vector_.ConvertToLinearConstraint(integer_variables_,
                                                      new_constraint_ub);

  std::string message;
  if (VLOG_IS_ON(1)) {
    // Unfortunately, we need to set this up before we std::move() it.
    message = absl::StrCat("LP exact dual ray not infeasible,",
                           " implied_lb: ", GetImpliedLowerBound(explanation),
                           " ub: ", explanation.ub.value());
  }

  // This should result in a conflict if the precision is good enough.
  if (!PropagateLpConstraint(std::move(explanation))) return false;

  VLOG(1) << message;
  return true;
}

int64_t LinearProgrammingConstraint::CalculateDegeneracy() {
  const glop::ColIndex num_vars = simplex_.GetProblemNumCols();
  int num_non_basic_with_zero_rc = 0;
  for (glop::ColIndex i(0); i < num_vars; ++i) {
    const double rc = simplex_.GetReducedCost(i);
    if (rc != 0.0) continue;
    if (simplex_.GetVariableStatus(i) == glop::VariableStatus::BASIC) {
      continue;
    }
    num_non_basic_with_zero_rc++;
  }
  const int64_t num_cols = simplex_.GetProblemNumCols().value();
  is_degenerate_ = num_non_basic_with_zero_rc >= 0.3 * num_cols;
  return num_non_basic_with_zero_rc;
}

void LinearProgrammingConstraint::ReducedCostStrengtheningDeductions(
    double cp_objective_delta) {
  deductions_.clear();

  // TRICKY: while simplex_.GetObjectiveValue() use the objective scaling factor
  // stored in the lp_data_, all the other functions like GetReducedCost() or
  // GetVariableValue() do not.
  const double lp_objective_delta =
      cp_objective_delta / lp_data_.objective_scaling_factor();
  const int num_vars = integer_variables_.size();
  for (int i = 0; i < num_vars; i++) {
    const IntegerVariable cp_var = integer_variables_[i];
    const glop::ColIndex lp_var = glop::ColIndex(i);
    const double rc = simplex_.GetReducedCost(lp_var);
    const double value = simplex_.GetVariableValue(lp_var);

    if (rc == 0.0) continue;
    const double lp_other_bound = value + lp_objective_delta / rc;
    const double cp_other_bound =
        scaler_.UnscaleVariableValue(lp_var, lp_other_bound);

    if (rc > kLpEpsilon) {
      const double ub = ToDouble(integer_trail_->UpperBound(cp_var));
      const double new_ub = std::floor(cp_other_bound + kCpEpsilon);
      if (new_ub < ub) {
        // TODO(user): Because rc > kLpEpsilon, the lower_bound of cp_var
        // will be part of the reason returned by FillReducedCostsReason(), but
        // we actually do not need it here. Same below.
        const IntegerValue new_ub_int(static_cast<IntegerValue>(new_ub));
        deductions_.push_back(IntegerLiteral::LowerOrEqual(cp_var, new_ub_int));
      }
    } else if (rc < -kLpEpsilon) {
      const double lb = ToDouble(integer_trail_->LowerBound(cp_var));
      const double new_lb = std::ceil(cp_other_bound - kCpEpsilon);
      if (new_lb > lb) {
        const IntegerValue new_lb_int(static_cast<IntegerValue>(new_lb));
        deductions_.push_back(
            IntegerLiteral::GreaterOrEqual(cp_var, new_lb_int));
      }
    }
  }
}

void LinearProgrammingConstraint::UpdateAverageReducedCosts() {
  const int num_vars = integer_variables_.size();
  if (sum_cost_down_.size() < num_vars) {
    sum_cost_down_.resize(num_vars, 0.0);
    num_cost_down_.resize(num_vars, 0);
    sum_cost_up_.resize(num_vars, 0.0);
    num_cost_up_.resize(num_vars, 0);
    rc_scores_.resize(num_vars, 0.0);
  }

  // Decay averages.
  num_calls_since_reduced_cost_averages_reset_++;
  if (num_calls_since_reduced_cost_averages_reset_ == 10000) {
    for (int i = 0; i < num_vars; i++) {
      sum_cost_up_[i] /= 2;
      num_cost_up_[i] /= 2;
      sum_cost_down_[i] /= 2;
      num_cost_down_[i] /= 2;
    }
    num_calls_since_reduced_cost_averages_reset_ = 0;
  }

  // Accumulate reduced costs of all unassigned variables.
  for (int i = 0; i < num_vars; i++) {
    const IntegerVariable var = integer_variables_[i];

    // Skip fixed variables.
    if (integer_trail_->IsFixed(var)) continue;

    // Skip reduced costs that are zero or close.
    const double rc = lp_reduced_cost_[i];
    if (std::abs(rc) < kCpEpsilon) continue;

    if (rc < 0.0) {
      sum_cost_down_[i] -= rc;
      num_cost_down_[i]++;
    } else {
      sum_cost_up_[i] += rc;
      num_cost_up_[i]++;
    }
  }

  // Tricky, we artificially reset the rc_rev_int_repository_ to level zero
  // so that the rev_rc_start_ is zero.
  rc_rev_int_repository_.SetLevel(0);
  rc_rev_int_repository_.SetLevel(trail_->CurrentDecisionLevel());
  rev_rc_start_ = 0;

  // Cache the new score (higher is better) using the average reduced costs
  // as a signal.
  positions_by_decreasing_rc_score_.clear();
  for (int i = 0; i < num_vars; i++) {
    // If only one direction exist, we takes its value divided by 2, so that
    // such variable should have a smaller cost than the min of the two side
    // except if one direction have a really high reduced costs.
    const double a_up =
        num_cost_up_[i] > 0 ? sum_cost_up_[i] / num_cost_up_[i] : 0.0;
    const double a_down =
        num_cost_down_[i] > 0 ? sum_cost_down_[i] / num_cost_down_[i] : 0.0;
    if (num_cost_down_[i] > 0 && num_cost_up_[i] > 0) {
      rc_scores_[i] = std::min(a_up, a_down);
    } else {
      rc_scores_[i] = 0.5 * (a_down + a_up);
    }

    // We ignore scores of zero (i.e. no data) and will follow the default
    // search heuristic if all variables are like this.
    if (rc_scores_[i] > 0.0) {
      positions_by_decreasing_rc_score_.push_back({-rc_scores_[i], i});
    }
  }
  std::sort(positions_by_decreasing_rc_score_.begin(),
            positions_by_decreasing_rc_score_.end());
}

std::function<IntegerLiteral()>
LinearProgrammingConstraint::HeuristicLpReducedCostAverageBranching() {
  return [this]() { return this->LPReducedCostAverageDecision(); };
}

IntegerLiteral LinearProgrammingConstraint::LPReducedCostAverageDecision() {
  // Select noninstantiated variable with highest positive average reduced cost.
  int selected_index = -1;
  const int size = positions_by_decreasing_rc_score_.size();
  rc_rev_int_repository_.SaveState(&rev_rc_start_);
  for (int i = rev_rc_start_; i < size; ++i) {
    const int index = positions_by_decreasing_rc_score_[i].second;
    const IntegerVariable var = integer_variables_[index];
    if (integer_trail_->IsFixed(var)) continue;
    selected_index = index;
    rev_rc_start_ = i;
    break;
  }

  if (selected_index == -1) return IntegerLiteral();
  const IntegerVariable var = integer_variables_[selected_index];

  // If ceil(value) is current upper bound, try var == upper bound first.
  // Guarding with >= prevents numerical problems.
  // With 0/1 variables, this will tend to try setting to 1 first,
  // which produces more shallow trees.
  const IntegerValue ub = integer_trail_->UpperBound(var);
  const IntegerValue value_ceil(
      std::ceil(this->GetSolutionValue(var) - kCpEpsilon));
  if (value_ceil >= ub) {
    return IntegerLiteral::GreaterOrEqual(var, ub);
  }

  // If floor(value) is current lower bound, try var == lower bound first.
  // Guarding with <= prevents numerical problems.
  const IntegerValue lb = integer_trail_->LowerBound(var);
  const IntegerValue value_floor(
      std::floor(this->GetSolutionValue(var) + kCpEpsilon));
  if (value_floor <= lb) {
    return IntegerLiteral::LowerOrEqual(var, lb);
  }

  // Here lb < value_floor <= value_ceil < ub.
  // Try the most promising split between var <= floor or var >= ceil.
  const double a_up =
      num_cost_up_[selected_index] > 0
          ? sum_cost_up_[selected_index] / num_cost_up_[selected_index]
          : 0.0;
  const double a_down =
      num_cost_down_[selected_index] > 0
          ? sum_cost_down_[selected_index] / num_cost_down_[selected_index]
          : 0.0;
  if (a_down < a_up) {
    return IntegerLiteral::LowerOrEqual(var, value_floor);
  } else {
    return IntegerLiteral::GreaterOrEqual(var, value_ceil);
  }
}

absl::Span<const glop::ColIndex> LinearProgrammingConstraint::IntegerLpRowCols(
    glop::RowIndex row) const {
  const int start = integer_lp_[row].start_in_buffer;
  const size_t num_terms = static_cast<size_t>(integer_lp_[row].num_terms);
  return {integer_lp_cols_.data() + start, num_terms};
}

absl::Span<const IntegerValue> LinearProgrammingConstraint::IntegerLpRowCoeffs(
    glop::RowIndex row) const {
  const int start = integer_lp_[row].start_in_buffer;
  const size_t num_terms = static_cast<size_t>(integer_lp_[row].num_terms);
  return {integer_lp_coeffs_.data() + start, num_terms};
}

}  // namespace sat
}  // namespace operations_research
