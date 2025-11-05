// Copyright 2010-2025 Google LLC
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

#ifndef ORTOOLS_SAT_INTEGER_EXPR_H_
#define ORTOOLS_SAT_INTEGER_EXPR_H_

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/types/span.h"
#include "ortools/sat/enforcement.h"
#include "ortools/sat/enforcement_helper.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_propagation.h"
#include "ortools/sat/model.h"
#include "ortools/sat/old_precedences_propagator.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// A really basic implementation of an upper-bounded sum of integer variables.
// The complexity is in O(num_variables) at each propagation.
//
// Note that we assume that there can be NO integer overflow. This must be
// checked at model validation time before this is even created. If
// use_int128 is true, then we actually do the computations that could overflow
// in 128 bits, so that we can deal with constraints that might overflow (like
// the one scaled from the LP relaxation). Note that we still use some
// preconditions, such that for each variable the difference between their
// bounds fit on an int64_t.
//
// TODO(user): Technically we could still have an int128 overflow since we
// sum n terms that cannot overflow but can still be pretty close to the limit.
// Make sure this never happens! For most problem though, because the variable
// bounds will be smaller than 10^9, we are pretty safe.
//
// TODO(user): If one has many such constraint, it will be more efficient to
// propagate all of them at once rather than doing it one at the time.
//
// TODO(user): Explore tree structure to get a log(n) complexity.
//
// TODO(user): When the variables are Boolean, use directly the pseudo-Boolean
// constraint implementation. But we do need support for enforcement literals
// there.
template <bool use_int128 = false>
class LinearConstraintPropagator : public PropagatorInterface,
                                   LazyReasonInterface {
 public:
  // If refied_literal is kNoLiteralIndex then this is a normal constraint,
  // otherwise we enforce the implication refied_literal => constraint is true.
  // Note that we don't do the reverse implication here, it is usually done by
  // another LinearConstraintPropagator constraint on the negated variables.
  LinearConstraintPropagator(absl::Span<const Literal> enforcement_literals,
                             absl::Span<const IntegerVariable> vars,
                             absl::Span<const IntegerValue> coeffs,
                             IntegerValue upper_bound, Model* model);

  // This version allow to std::move the memory from the LinearConstraint
  // directly. It Only uses the upper bound. Id does not support
  // enforcement_literals.
  LinearConstraintPropagator(LinearConstraint ct, Model* model);

  std::string LazyReasonName() const override {
    return use_int128 ? "IntegerSumLE128" : "IntegerSumLE";
  }

  // We propagate:
  // - If the sum of the individual lower-bound is > upper_bound, we fail.
  // - For all i, upper-bound of i
  //      <= upper_bound - Sum {individual lower-bound excluding i).
  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

  // Same as Propagate() but only consider current root level bounds. This is
  // mainly useful for the LP propagator since it can find relevant optimal
  // really late in the search tree.
  bool PropagateAtLevelZero();

  // This is a pretty usage specific function. Returns the implied lower bound
  // on target_var if the given integer literal is false (resp. true). If the
  // variables do not appear both in the linear inequality, this returns two
  // kMinIntegerValue.
  std::pair<IntegerValue, IntegerValue> ConditionalLb(
      IntegerLiteral integer_literal, IntegerVariable target_var) const;

  // For LazyReasonInterface.
  void Explain(int id, IntegerValue propagation_slack,
               IntegerVariable var_to_explain, int trail_index,
               std::vector<Literal>* literals_reason,
               std::vector<int>* trail_indices_reason) final;

 private:
  // Fills integer_reason_ with all the current lower_bounds. The real
  // explanation may require removing one of them, but as an optimization, we
  // always keep all the IntegerLiteral in integer_reason_, and swap them as
  // needed just before pushing something.
  void FillIntegerReason();

  const IntegerValue upper_bound_;

  // To gain a bit on memory (since we might have many linear constraint),
  // we share this amongst all of them. Note that this is not accessed by
  // two different thread though. Also the vector are only used as "temporary"
  // so they are okay to be shared.
  struct Shared {
    explicit Shared(Model* model)
        : assignment(model->GetOrCreate<Trail>()->Assignment()),
          integer_trail(model->GetOrCreate<IntegerTrail>()),
          time_limit(model->GetOrCreate<TimeLimit>()),
          rev_int_repository(model->GetOrCreate<RevIntRepository>()),
          rev_integer_value_repository(
              model->GetOrCreate<RevIntegerValueRepository>()) {}

    const VariablesAssignment& assignment;
    IntegerTrail* integer_trail;
    TimeLimit* time_limit;
    RevIntRepository* rev_int_repository;
    RevIntegerValueRepository* rev_integer_value_repository;

    // Parallel vectors.
    std::vector<IntegerLiteral> integer_reason;
    std::vector<IntegerValue> reason_coeffs;
  };
  Shared* shared_;

  // Reversible sum of the lower bound of the fixed variables.
  bool is_registered_ = false;
  IntegerValue rev_lb_fixed_vars_;

  // Reversible number of fixed variables.
  int rev_num_fixed_vars_;

  // Those vectors are shuffled during search to ensure that the variables
  // (resp. coefficients) contained in the range [0, rev_num_fixed_vars_) of
  // vars_ (resp. coeffs_) are fixed (resp. belong to fixed variables).
  const int size_;
  const std::unique_ptr<IntegerVariable[]> vars_;
  const std::unique_ptr<IntegerValue[]> coeffs_;
  const std::unique_ptr<IntegerValue[]> max_variations_;

  // This is just the negation of the enforcement literal and it never changes.
  std::vector<Literal> literal_reason_;
};

using IntegerSumLE = LinearConstraintPropagator<false>;
using IntegerSumLE128 = LinearConstraintPropagator<true>;

// Explicit instantiations in integer_expr.cc.
extern template class LinearConstraintPropagator<true>;
extern template class LinearConstraintPropagator<false>;

// This assumes target = SUM_i coeffs[i] * vars[i], and detects that the target
// must be of the form (a*X + b).
//
// This propagator is quite specific and runs only at level zero. For now, this
// is mainly used for the objective variable. As we fix terms with high
// objective coefficient, it is possible the only terms left have a common
// divisor. This close app2-2.mps in less than a second instead of running
// forever to prove the optimal (in single thread).
class LevelZeroEquality : PropagatorInterface {
 public:
  LevelZeroEquality(IntegerVariable target,
                    const std::vector<IntegerVariable>& vars,
                    const std::vector<IntegerValue>& coeffs, Model* model);

  bool Propagate() final;

 private:
  const IntegerVariable target_;
  const std::vector<IntegerVariable> vars_;
  const std::vector<IntegerValue> coeffs_;

  IntegerValue gcd_ = IntegerValue(1);

  Trail* trail_;
  IntegerTrail* integer_trail_;
};

// A min (resp max) constraint of the form min == MIN(vars) can be decomposed
// into two inequalities:
//   1/ min <= MIN(vars), which is the same as for all v in vars, "min <= v".
//      This can be taken care of by the LowerOrEqual(min, v) constraint.
//   2/ min >= MIN(vars).
//
// And in turn, 2/ can be decomposed in:
//   a) lb(min) >= lb(MIN(vars)) = MIN(lb(var));
//   b) ub(min) >= ub(MIN(vars)) and we can't propagate anything here unless
//      there is just one possible variable 'v' that can be the min:
//         for all u != v, lb(u) > ub(min);
//      In this case, ub(min) >= ub(v).
//
// This constraint take care of a) and b). That is:
// - If the min of the lower bound of the vars increase, then the lower bound of
//   the min_var will be >= to it.
// - If there is only one candidate for the min, then if the ub(min) decrease,
//   the ub of the only candidate will be <= to it.
//
// Complexity: This is a basic implementation in O(num_vars) on each call to
// Propagate(), which will happen each time one or more variables in vars_
// changed.
//
// TODO(user): Implement a more efficient algorithm when the need arise.
class MinPropagator : public PropagatorInterface {
 public:
  MinPropagator(std::vector<AffineExpression> vars, AffineExpression min_var,
                IntegerTrail* integer_trail);

  // This type is neither copyable nor movable.
  MinPropagator(const MinPropagator&) = delete;
  MinPropagator& operator=(const MinPropagator&) = delete;

  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  const std::vector<AffineExpression> vars_;
  const AffineExpression min_var_;
  IntegerTrail* integer_trail_;

  std::vector<IntegerLiteral> integer_reason_;
};

// Same as MinPropagator except this works on min = MIN(exprs) where exprs are
// linear expressions. It uses IntegerSumLE to propagate bounds on the exprs.
// Assumes Canonical expressions (all positive coefficients).
class GreaterThanMinOfExprsPropagator : public PropagatorInterface,
                                        LazyReasonInterface {
 public:
  GreaterThanMinOfExprsPropagator(
      absl::Span<const Literal> enforcement_literals,
      std::vector<LinearExpression> exprs, IntegerVariable min_var,
      Model* model);
  GreaterThanMinOfExprsPropagator(const GreaterThanMinOfExprsPropagator&) =
      delete;
  GreaterThanMinOfExprsPropagator& operator=(
      const GreaterThanMinOfExprsPropagator&) = delete;

  std::string LazyReasonName() const override {
    return "GreaterThanMinOfExprsPropagator";
  }

  bool Propagate() final;

  // For LazyReasonInterface.
  void Explain(int id, IntegerValue propagation_slack,
               IntegerVariable var_to_explain, int trail_index,
               std::vector<Literal>* literals_reason,
               std::vector<int>* trail_indices_reason) final;

 private:
  int RegisterWith(GenericLiteralWatcher* watcher);

  // Lighter version of IntegerSumLE. This uses the current value of
  // integer_reason_ in addition to the reason for propagating the linear
  // constraint. The coeffs are assumed to be positive here.
  bool PropagateLinearUpperBound(int id, absl::Span<const IntegerVariable> vars,
                                 absl::Span<const IntegerValue> coeffs,
                                 IntegerValue upper_bound);

  const std::vector<LinearExpression> exprs_;
  const IntegerVariable min_var_;
  std::vector<IntegerValue> expr_lbs_;
  Model* model_;
  IntegerTrail& integer_trail_;
  EnforcementHelper& enforcement_helper_;
  EnforcementId enforcement_id_;
  std::vector<IntegerValue> max_variations_;
  std::vector<IntegerValue> reason_coeffs_;
  std::vector<IntegerLiteral> local_reason_;
  std::vector<IntegerLiteral> integer_reason_for_unique_candidate_;
  int rev_unique_candidate_ = 0;
};

// Propagates a * b = p.
//
// The bounds [min, max] of a and b will be propagated perfectly, but not
// the bounds on p as this require more complex arithmetics.
class ProductPropagator : public PropagatorInterface {
 public:
  ProductPropagator(absl::Span<const Literal> enforcement_literals,
                    AffineExpression a, AffineExpression b, AffineExpression p,
                    Model* model);

  // This type is neither copyable nor movable.
  ProductPropagator(const ProductPropagator&) = delete;
  ProductPropagator& operator=(const ProductPropagator&) = delete;

  bool Propagate() final;

 private:
  int RegisterWith(GenericLiteralWatcher* watcher);

  // Maybe replace a_, b_ or c_ by their negation to simplify the cases.
  bool CanonicalizeCases();

  // Special case when all are >= 0.
  // We use faster code and better reasons than the generic code.
  bool PropagateWhenAllNonNegative();

  // Internal helper, see code for more details.
  bool PropagateMaxOnPositiveProduct(AffineExpression a, AffineExpression b,
                                     IntegerValue min_p, IntegerValue max_p);

  // Note that we might negate any two terms in CanonicalizeCases() during
  // each propagation. This is fine.
  AffineExpression a_;
  AffineExpression b_;
  AffineExpression p_;
  const IntegerTrail& integer_trail_;
  EnforcementHelper& enforcement_helper_;
  EnforcementId enforcement_id_;
};

// Propagates num / denom = div. Basic version, we don't extract any special
// cases, and we only propagates the bounds. It expects denom to be > 0.
//
// TODO(user): Deal with overflow.
class DivisionPropagator : public PropagatorInterface {
 public:
  DivisionPropagator(absl::Span<const Literal> enforcement_literals,
                     AffineExpression num, AffineExpression denom,
                     AffineExpression div, Model* model);

  // This type is neither copyable nor movable.
  DivisionPropagator(const DivisionPropagator&) = delete;
  DivisionPropagator& operator=(const DivisionPropagator&) = delete;

  bool Propagate() final;

 private:
  int RegisterWith(GenericLiteralWatcher* watcher);

  // Propagates the fact that the signs of each domain, if fixed, are
  // compatible.
  bool PropagateSigns(AffineExpression num, AffineExpression denom,
                      AffineExpression div);

  // If both num and div >= 0, we can propagate their upper bounds.
  bool PropagateUpperBounds(AffineExpression num, AffineExpression denom,
                            AffineExpression div);

  // When the sign of all 3 expressions are fixed, we can do morel propagation.
  //
  // By using negated expressions, we can make sure the domains of num, denom,
  // and div are positive.
  bool PropagatePositiveDomains(AffineExpression num, AffineExpression denom,
                                AffineExpression div);

  AffineExpression num_;
  AffineExpression denom_;
  const AffineExpression div_;
  const AffineExpression negated_div_;
  const IntegerTrail& integer_trail_;
  EnforcementHelper& enforcement_helper_;
  EnforcementId enforcement_id_;
};

// Propagates var_a / cst_b = var_c. Basic version, we don't extract any special
// cases, and we only propagates the bounds. cst_b must be > 0.
class FixedDivisionPropagator : public PropagatorInterface {
 public:
  FixedDivisionPropagator(absl::Span<const Literal> enforcement_literals,
                          AffineExpression a, IntegerValue b,
                          AffineExpression c, Model* model);

  // This type is neither copyable nor movable.
  FixedDivisionPropagator(const FixedDivisionPropagator&) = delete;
  FixedDivisionPropagator& operator=(const FixedDivisionPropagator&) = delete;

  bool Propagate() final;

 private:
  int RegisterWith(GenericLiteralWatcher* watcher);

  const AffineExpression a_;
  const IntegerValue b_;
  const AffineExpression c_;
  const IntegerTrail& integer_trail_;
  EnforcementHelper& enforcement_helper_;
  EnforcementId enforcement_id_;
};

// Propagates target == expr % mod. Basic version, we don't extract any special
// cases, and we only propagates the bounds. mod must be > 0.
class FixedModuloPropagator : public PropagatorInterface {
 public:
  FixedModuloPropagator(absl::Span<const Literal> enforcement_literals,
                        AffineExpression expr, IntegerValue mod,
                        AffineExpression target, Model* model);

  // This type is neither copyable nor movable.
  FixedModuloPropagator(const FixedModuloPropagator&) = delete;
  FixedModuloPropagator& operator=(const FixedModuloPropagator&) = delete;

  bool Propagate() final;

 private:
  int RegisterWith(GenericLiteralWatcher* watcher);

  bool PropagateWhenFalseAndTargetIsPositive(AffineExpression expr,
                                             AffineExpression target);
  bool PropagateWhenFalseAndTargetDomainContainsZero();
  bool PropagateSignsAndTargetRange();
  bool PropagateBoundsWhenExprIsNonNegative(AffineExpression expr,
                                            AffineExpression target);
  bool PropagateOuterBounds();

  const AffineExpression expr_;
  const IntegerValue mod_;
  const AffineExpression target_;
  const AffineExpression negated_expr_;
  const AffineExpression negated_target_;
  const IntegerTrail& integer_trail_;
  EnforcementHelper& enforcement_helper_;
  EnforcementId enforcement_id_;
};

// Propagates x * x = s.
// TODO(user): Only works for x nonnegative.
class SquarePropagator : public PropagatorInterface {
 public:
  SquarePropagator(absl::Span<const Literal> enforcement_literals,
                   AffineExpression x, AffineExpression s, Model* model);

  // This type is neither copyable nor movable.
  SquarePropagator(const SquarePropagator&) = delete;
  SquarePropagator& operator=(const SquarePropagator&) = delete;

  bool Propagate() final;

 private:
  int RegisterWith(GenericLiteralWatcher* watcher);

  const AffineExpression x_;
  const AffineExpression s_;
  const IntegerTrail& integer_trail_;
  EnforcementHelper& enforcement_helper_;
  EnforcementId enforcement_id_;
};

// =============================================================================
// Model based functions.
// =============================================================================

// Weighted sum <= constant.
template <typename VectorInt>
inline std::function<void(Model*)> WeightedSumLowerOrEqual(
    absl::Span<const IntegerVariable> vars, const VectorInt& coefficients,
    int64_t upper_bound) {
  return [=, vars = std::vector<IntegerVariable>(vars.begin(), vars.end())](
             Model* model) {
    return AddWeightedSumLowerOrEqual({}, vars, coefficients, upper_bound,
                                      model);
  };
}

// Weighted sum >= constant.
template <typename VectorInt>
inline std::function<void(Model*)> WeightedSumGreaterOrEqual(
    absl::Span<const IntegerVariable> vars, const VectorInt& coefficients,
    int64_t lower_bound) {
  // We just negate everything and use an <= constraints.
  std::vector<int64_t> negated_coeffs(coefficients.begin(), coefficients.end());
  for (int64_t& ref : negated_coeffs) ref = -ref;
  return WeightedSumLowerOrEqual(vars, negated_coeffs, -lower_bound);
}

// Weighted sum == constant.
template <typename VectorInt>
inline std::function<void(Model*)> FixedWeightedSum(
    absl::Span<const IntegerVariable> vars, const VectorInt& coefficients,
    int64_t value) {
  return [=, vars = std::vector<IntegerVariable>(vars.begin(), vars.end())](
             Model* model) {
    model->Add(WeightedSumGreaterOrEqual(vars, coefficients, value));
    model->Add(WeightedSumLowerOrEqual(vars, coefficients, value));
  };
}

// enforcement_literals => sum <= upper_bound
inline void AddWeightedSumLowerOrEqual(
    absl::Span<const Literal> enforcement_literals,
    absl::Span<const IntegerVariable> vars,
    absl::Span<const int64_t> coefficients, int64_t upper_bound, Model* model) {
  // Linear1.
  DCHECK_GE(vars.size(), 1);
  if (vars.size() == 1) {
    DCHECK_NE(coefficients[0], 0);
    IntegerVariable var = vars[0];
    IntegerValue coeff(coefficients[0]);
    if (coeff < 0) {
      var = NegationOf(var);
      coeff = -coeff;
    }
    const IntegerValue rhs = FloorRatio(IntegerValue(upper_bound), coeff);
    if (enforcement_literals.empty()) {
      model->Add(LowerOrEqual(var, rhs.value()));
    } else {
      model->Add(Implication(enforcement_literals,
                             IntegerLiteral::LowerOrEqual(var, rhs)));
    }
    return;
  }

  // Detect precedences with 2 and 3 terms.
  const SatParameters& params = *model->GetOrCreate<SatParameters>();
  if (!params.new_linear_propagation()) {
    if (vars.size() == 2 && (coefficients[0] == 1 || coefficients[0] == -1) &&
        (coefficients[1] == 1 || coefficients[1] == -1)) {
      AddConditionalSum2LowerOrEqual(
          enforcement_literals,
          coefficients[0] == 1 ? vars[0] : NegationOf(vars[0]),
          coefficients[1] == 1 ? vars[1] : NegationOf(vars[1]), upper_bound,
          model);
      return;
    }
    if (vars.size() == 3 && (coefficients[0] == 1 || coefficients[0] == -1) &&
        (coefficients[1] == 1 || coefficients[1] == -1) &&
        (coefficients[2] == 1 || coefficients[2] == -1)) {
      AddConditionalSum3LowerOrEqual(
          enforcement_literals,
          coefficients[0] == 1 ? vars[0] : NegationOf(vars[0]),
          coefficients[1] == 1 ? vars[1] : NegationOf(vars[1]),
          coefficients[2] == 1 ? vars[2] : NegationOf(vars[2]), upper_bound,
          model);
      return;
    }
  }

  // If value == min(expression), then we can avoid creating the sum.
  //
  // TODO(user): Deal with the case with no enforcement literal, in case the
  // presolve was turned off?
  if (!enforcement_literals.empty()) {
    IntegerValue expression_min(0);
    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    for (int i = 0; i < vars.size(); ++i) {
      expression_min +=
          coefficients[i] * (coefficients[i] >= 0
                                 ? integer_trail->LevelZeroLowerBound(vars[i])
                                 : integer_trail->LevelZeroUpperBound(vars[i]));
    }
    if (expression_min == upper_bound) {
      // Tricky: as we create integer literal, we might propagate stuff and
      // the bounds might change, so if the expression_min increase with the
      // bound we use, then the literal must be false.
      IntegerValue non_cached_min;
      for (int i = 0; i < vars.size(); ++i) {
        if (coefficients[i] > 0) {
          const IntegerValue lb = integer_trail->LevelZeroLowerBound(vars[i]);
          non_cached_min += coefficients[i] * lb;
          model->Add(Implication(enforcement_literals,
                                 IntegerLiteral::LowerOrEqual(vars[i], lb)));
        } else if (coefficients[i] < 0) {
          const IntegerValue ub = integer_trail->LevelZeroUpperBound(vars[i]);
          non_cached_min += coefficients[i] * ub;
          model->Add(Implication(enforcement_literals,
                                 IntegerLiteral::GreaterOrEqual(vars[i], ub)));
        }
      }
      if (non_cached_min > expression_min) {
        std::vector<Literal> clause;
        for (const Literal l : enforcement_literals) {
          clause.push_back(l.Negated());
        }
        model->Add(ClauseConstraint(clause));
      }
      return;
    }
  }

  if (params.new_linear_propagation()) {
    const bool ok = model->GetOrCreate<LinearPropagator>()->AddConstraint(
        enforcement_literals, vars,
        std::vector<IntegerValue>(coefficients.begin(), coefficients.end()),
        IntegerValue(upper_bound));
    if (!ok) {
      auto* sat_solver = model->GetOrCreate<SatSolver>();
      if (sat_solver->CurrentDecisionLevel() == 0) {
        sat_solver->NotifyThatModelIsUnsat();
      } else {
        LOG(FATAL) << "We currently do not support adding conflicting "
                      "constraint at positive level.";
      }
    }
  } else {
    IntegerSumLE* constraint = new IntegerSumLE(
        enforcement_literals, vars,
        std::vector<IntegerValue>(coefficients.begin(), coefficients.end()),
        IntegerValue(upper_bound), model);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  }
}

// enforcement_literals => sum >= lower_bound
inline void AddWeightedSumGreaterOrEqual(
    absl::Span<const Literal> enforcement_literals,
    absl::Span<const IntegerVariable> vars,
    absl::Span<const int64_t> coefficients, int64_t lower_bound, Model* model) {
  // We just negate everything and use an <= constraint.
  std::vector<int64_t> negated_coeffs(coefficients.begin(), coefficients.end());
  for (int64_t& ref : negated_coeffs) ref = -ref;
  AddWeightedSumLowerOrEqual(enforcement_literals, vars, negated_coeffs,
                             -lower_bound, model);
}

// TODO(user): Delete once Telamon use new function.
inline std::function<void(Model*)> ConditionalWeightedSumLowerOrEqual(
    absl::Span<const Literal> enforcement_literals,
    absl::Span<const IntegerVariable> vars,
    absl::Span<const int64_t> coefficients, int64_t upper_bound) {
  return [=, vars = std::vector<IntegerVariable>(vars.begin(), vars.end()),
          coefficients =
              std::vector<int64_t>(coefficients.begin(), coefficients.end()),
          enforcement_literals =
              std::vector<Literal>(enforcement_literals.begin(),
                                   enforcement_literals.end())](Model* model) {
    AddWeightedSumLowerOrEqual(enforcement_literals, vars, coefficients,
                               upper_bound, model);
  };
}
inline std::function<void(Model*)> ConditionalWeightedSumGreaterOrEqual(
    absl::Span<const Literal> enforcement_literals,
    absl::Span<const IntegerVariable> vars,
    absl::Span<const int64_t> coefficients, int64_t upper_bound) {
  return [=,
          coefficients =
              std::vector<int64_t>(coefficients.begin(), coefficients.end()),
          vars = std::vector<IntegerVariable>(vars.begin(), vars.end()),
          enforcement_literals =
              std::vector<Literal>(enforcement_literals.begin(),
                                   enforcement_literals.end())](Model* model) {
    AddWeightedSumGreaterOrEqual(enforcement_literals, vars, coefficients,
                                 upper_bound, model);
  };
}

// LinearConstraint version.
inline void LoadConditionalLinearConstraint(
    const absl::Span<const Literal> enforcement_literals,
    const LinearConstraint& cst, Model* model) {
  if (cst.num_terms == 0) {
    if (cst.lb <= 0 && cst.ub >= 0) return;

    // The enforcement literals cannot be all at true.
    std::vector<Literal> clause;
    for (const Literal lit : enforcement_literals) {
      clause.push_back(lit.Negated());
    }
    return model->Add(ClauseConstraint(clause));
  }

  // TODO(user): Remove the conversion!
  std::vector<IntegerVariable> vars(cst.num_terms);
  std::vector<int64_t> coeffs(cst.num_terms);
  for (int i = 0; i < cst.num_terms; ++i) {
    vars[i] = cst.vars[i];
    coeffs[i] = cst.coeffs[i].value();
  }

  if (cst.ub < kMaxIntegerValue) {
    AddWeightedSumLowerOrEqual(enforcement_literals, vars, coeffs,
                               cst.ub.value(), model);
  }
  if (cst.lb > kMinIntegerValue) {
    AddWeightedSumGreaterOrEqual(enforcement_literals, vars, coeffs,
                                 cst.lb.value(), model);
  }
}

inline void LoadLinearConstraint(const LinearConstraint& cst, Model* model) {
  LoadConditionalLinearConstraint({}, cst, model);
}

inline void AddConditionalAffinePrecedence(
    const absl::Span<const Literal> enforcement_literals, AffineExpression left,
    AffineExpression right, Model* model) {
  LinearConstraintBuilder builder(model, kMinIntegerValue, 0);
  builder.AddTerm(left, 1);
  builder.AddTerm(right, -1);
  LoadConditionalLinearConstraint(enforcement_literals, builder.Build(), model);
}

// Model-based function to create an IntegerVariable that corresponds to the
// given weighted sum of other IntegerVariables.
//
// Note that this is templated so that it can seamlessly accept vector<int> or
// vector<int64_t>.
//
// TODO(user): invert the coefficients/vars arguments.
template <typename VectorInt>
inline std::function<IntegerVariable(Model*)> NewWeightedSum(
    const VectorInt& coefficients, const std::vector<IntegerVariable>& vars) {
  return [=](Model* model) {
    std::vector<IntegerVariable> new_vars = vars;
    // To avoid overflow in the FixedWeightedSum() constraint, we need to
    // compute the basic bounds on the sum.
    //
    // TODO(user): deal with overflow here too!
    int64_t sum_lb(0);
    int64_t sum_ub(0);
    for (int i = 0; i < new_vars.size(); ++i) {
      if (coefficients[i] > 0) {
        sum_lb += coefficients[i] * model->Get(LowerBound(new_vars[i]));
        sum_ub += coefficients[i] * model->Get(UpperBound(new_vars[i]));
      } else {
        sum_lb += coefficients[i] * model->Get(UpperBound(new_vars[i]));
        sum_ub += coefficients[i] * model->Get(LowerBound(new_vars[i]));
      }
    }

    const IntegerVariable sum = model->Add(NewIntegerVariable(sum_lb, sum_ub));
    new_vars.push_back(sum);
    std::vector<int64_t> new_coeffs(coefficients.begin(), coefficients.end());
    new_coeffs.push_back(-1);
    model->Add(FixedWeightedSum(new_vars, new_coeffs, 0));
    return sum;
  };
}

// Expresses the fact that an existing integer variable is equal to the minimum
// of linear expressions. Assumes Canonical expressions (all positive
// coefficients).
inline void AddIsEqualToMinOf(
    const absl::Span<const Literal> enforcement_literals,
    const LinearExpression& min_expr, std::vector<LinearExpression> exprs,
    Model* model) {
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();

  IntegerVariable min_var;
  if (min_expr.vars.size() == 1 && std::abs(min_expr.coeffs[0].value()) == 1 &&
      min_expr.offset == 0) {
    if (min_expr.coeffs[0].value() == 1) {
      min_var = min_expr.vars[0];
    } else {
      min_var = NegationOf(min_expr.vars[0]);
    }
  } else {
    // Create a new variable if the expression is not just a single variable.
    IntegerValue min_lb = min_expr.Min(*integer_trail);
    IntegerValue min_ub = min_expr.Max(*integer_trail);
    min_var = integer_trail->AddIntegerVariable(min_lb, min_ub);

    // min_var = min_expr
    LinearConstraintBuilder builder(0, 0);
    builder.AddLinearExpression(min_expr, 1);
    builder.AddTerm(min_var, -1);
    LoadLinearConstraint(builder.Build(), model);
  }

  // Add for all i, enforcement_literals => min <= exprs[i].
  for (const LinearExpression& expr : exprs) {
    LinearConstraintBuilder builder(0, kMaxIntegerValue);
    builder.AddLinearExpression(expr, 1);
    builder.AddTerm(min_var, -1);
    LoadConditionalLinearConstraint(enforcement_literals, builder.Build(),
                                    model);
  }

  GreaterThanMinOfExprsPropagator* constraint =
      new GreaterThanMinOfExprsPropagator(enforcement_literals,
                                          std::move(exprs), min_var, model);
  model->TakeOwnership(constraint);
}

ABSL_DEPRECATED("Use AddIsEqualToMinOf() instead.")
inline std::function<void(Model*)> IsEqualToMinOf(
    const LinearExpression& min_expr,
    const std::vector<LinearExpression>& exprs) {
  return [&](Model* model) {
    AddIsEqualToMinOf(/*enforcement_literals=*/{}, min_expr, exprs, model);
  };
}

// Adds the constraint: a * b = p.
inline std::function<void(Model*)> ProductConstraint(
    absl::Span<const Literal> enforcement_literals, AffineExpression a,
    AffineExpression b, AffineExpression p) {
  return [=](Model* model) {
    const IntegerTrail& integer_trail = *model->GetOrCreate<IntegerTrail>();
    // TODO(user): return early if constraint is never enforced.
    if (a == b) {
      if (integer_trail.LowerBound(a) >= 0) {
        model->TakeOwnership(
            new SquarePropagator(enforcement_literals, a, p, model));
        return;
      }
      if (integer_trail.UpperBound(a) <= 0) {
        model->TakeOwnership(
            new SquarePropagator(enforcement_literals, a.Negated(), p, model));
        return;
      }
    }
    model->TakeOwnership(
        new ProductPropagator(enforcement_literals, a, b, p, model));
  };
}

// Adds the constraint: num / denom = div. (denom > 0).
inline std::function<void(Model*)> DivisionConstraint(
    absl::Span<const Literal> enforcement_literals, AffineExpression num,
    AffineExpression denom, AffineExpression div) {
  return [=](Model* model) {
    const IntegerTrail& integer_trail = *model->GetOrCreate<IntegerTrail>();
    // TODO(user): return early if constraint is never enforced.
    DivisionPropagator* constraint;
    if (integer_trail.UpperBound(denom) < 0) {
      constraint = new DivisionPropagator(enforcement_literals, num.Negated(),
                                          denom.Negated(), div, model);
    } else {
      constraint =
          new DivisionPropagator(enforcement_literals, num, denom, div, model);
    }
    model->TakeOwnership(constraint);
  };
}

// Adds the constraint: a / b = c where b is a constant.
inline std::function<void(Model*)> FixedDivisionConstraint(
    absl::Span<const Literal> enforcement_literals, AffineExpression a,
    IntegerValue b, AffineExpression c) {
  return [=](Model* model) {
    // TODO(user): return early if constraint is never enforced.
    FixedDivisionPropagator* constraint =
        b > 0
            ? new FixedDivisionPropagator(enforcement_literals, a, b, c, model)
            : new FixedDivisionPropagator(enforcement_literals, a.Negated(), -b,
                                          c, model);
    model->TakeOwnership(constraint);
  };
}

// Adds the constraint: a % b = c where b is a constant.
inline std::function<void(Model*)> FixedModuloConstraint(
    absl::Span<const Literal> enforcement_literals, AffineExpression a,
    IntegerValue b, AffineExpression c) {
  return [=](Model* model) {
    model->TakeOwnership(
        new FixedModuloPropagator(enforcement_literals, a, b, c, model));
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_INTEGER_EXPR_H_
