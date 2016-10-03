// Copyright 2010-2014 Google
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

#ifndef OR_TOOLS_SAT_INTEGER_EXPR_H_
#define OR_TOOLS_SAT_INTEGER_EXPR_H_

#include "sat/integer.h"
#include "sat/model.h"
#include "sat/precedences.h"
#include "sat/sat_base.h"

namespace operations_research {
namespace sat {

// A really basic implementation of an upper-bounded sum of integer variables.
// The complexity is in O(num_variables) at each propagation.
//
// TODO(user): If one has many such constraint, it will be more efficient to
// propagate all of them at once rather than doing it one at the time.
//
// TODO(user): Handle integer overflow! The easiest is probably to check at
// construction that overflow cannot happen.
//
// TODO(user): Explore tree structure to get a log(n) complexity.
//
// TODO(user): When the variables are Boolean, use directly the pseudo-Boolean
// constraint implementation.
class IntegerSumLE : public PropagatorInterface {
 public:
  IntegerSumLE(const std::vector<IntegerVariable>& vars,
               const std::vector<IntegerValue>& coefficients,
               IntegerValue upper_bound, IntegerTrail* integer_trail);

  // We propagate:
  // - If the sum of the individual lower-bound is > upper_bound, we fail.
  // - For all i, upper-bound of i
  //      <= upper_bound - Sum {individual lower-bound excluding i).
  bool Propagate(Trail* trail) final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  const IntegerValue upper_bound_;
  std::vector<IntegerVariable> vars_;
  std::vector<IntegerValue> coeffs_;
  IntegerTrail* integer_trail_;

  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> integer_reason_;

  DISALLOW_COPY_AND_ASSIGN(IntegerSumLE);
};

// A min (resp max) contraint of the form min == MIN(vars) can be decomposed
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
  MinPropagator(const std::vector<IntegerVariable>& vars, IntegerVariable min_var,
                IntegerTrail* integer_trail);

  bool Propagate(Trail* trail) final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  const std::vector<IntegerVariable> vars_;
  const IntegerVariable min_var_;
  IntegerTrail* integer_trail_;

  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> integer_reason_;

  DISALLOW_COPY_AND_ASSIGN(MinPropagator);
};

// Propagate the fact that a given integer variable is equal to exactly one
// element out of a given set of values. Each value is selected or not by a
// literal being true.
//
// Note that the fact that exactly one "selector" should be true is not enforced
// here. This class just propagate the directions:
// - selector is false => potentially restrict the [lb, ub] of var.
// - [lb, ub] change => more selector can be set to false.
class IsOneOfPropagator : public PropagatorInterface {
 public:
  IsOneOfPropagator(IntegerVariable var, const std::vector<Literal>& selectors,
                    const std::vector<IntegerValue>& values,
                    IntegerTrail* integer_trail);

  bool Propagate(Trail* trail) final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  const IntegerVariable var_;
  const std::vector<Literal> selectors_;
  const std::vector<IntegerValue> values_;
  IntegerTrail* integer_trail_;

  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> integer_reason_;

  DISALLOW_COPY_AND_ASSIGN(IsOneOfPropagator);
};

// =============================================================================
// Model based functions.
// =============================================================================

// Weighted sum <= constant.
template <typename VectorInt>
inline std::function<void(Model*)> WeightedSumLowerOrEqual(
    const std::vector<IntegerVariable>& vars, const VectorInt& coefficients,
    int64 upper_bound) {
  return [=](Model* model) {
    IntegerSumLE* constraint = new IntegerSumLE(
        vars, std::vector<IntegerValue>(coefficients.begin(), coefficients.end()),
        IntegerValue(upper_bound), model->GetOrCreate<IntegerTrail>());
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

// Weighted sum >= constant.
template <typename VectorInt>
inline std::function<void(Model*)> WeightedSumGreaterOrEqual(
    const std::vector<IntegerVariable>& vars, const VectorInt& coefficients,
    int64 lower_bound) {
  return [=](Model* model) {
    // We just negate everything and use an IntegerSumLE() constraints.
    std::vector<IntegerValue> new_coeffs(coefficients.begin(), coefficients.end());
    for (IntegerValue& ref : new_coeffs) ref = -ref;
    IntegerSumLE* constraint =
        new IntegerSumLE(vars, new_coeffs, IntegerValue(-lower_bound),
                         model->GetOrCreate<IntegerTrail>());
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

// Weighted sum == constant
template <typename VectorInt>
inline std::function<void(Model*)> FixedWeightedSum(
    const std::vector<IntegerVariable>& vars, const VectorInt& coefficients,
    int64 value) {
  return [=](Model* model) {
    model->Add(WeightedSumGreaterOrEqual(vars, coefficients, value));
    model->Add(WeightedSumLowerOrEqual(vars, coefficients, value));
  };
}

// Model-based function to create an IntegerVariable that corresponds to the
// given weighted sum of other IntegerVariables.
//
// Note that this is templated so that it can seemlessly accept std::vector<int>,
// std::vector<int64> or std::vector<IntegerValue>!
//
// TODO(user): invert the coefficients/vars arguments.
template <typename VectorInt>
inline std::function<IntegerVariable(Model*)> NewWeightedSum(
    const VectorInt& coefficients, const std::vector<IntegerVariable>& vars) {
  return [=](Model* model) {
    std::vector<IntegerVariable> new_vars = vars;
    std::vector<IntegerValue> new_coeffs(coefficients.begin(), coefficients.end());

    // To avoid overflow in the FixedWeightedSum() constraint, we need to
    // compute the basic bounds on the sum.
    //
    // TODO(user): deal with overflow here too!
    int64 sum_lb(0);
    int64 sum_ub(0);
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
    new_coeffs.push_back(IntegerValue(-1));

    model->Add(FixedWeightedSum(new_vars, new_coeffs, 0));
    return sum;
  };
}

// Expresses the fact that an existing integer variable is equal to the minimum
// of other integer variables.
inline std::function<void(Model*)> IsEqualToMinOf(
    IntegerVariable min_var, const std::vector<IntegerVariable>& vars) {
  return [=](Model* model) {
    for (const IntegerVariable& var : vars) {
      model->Add(LowerOrEqual(min_var, var));
    }

    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    MinPropagator* constraint = new MinPropagator(vars, min_var, integer_trail);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

// Expresses the fact that an existing integer variable is equal to the maximum
// of other integer variables.
inline std::function<void(Model*)> IsEqualToMaxOf(
    IntegerVariable max_var, const std::vector<IntegerVariable>& vars) {
  return [=](Model* model) {
    std::vector<IntegerVariable> negated_vars;
    for (const IntegerVariable& var : vars) {
      negated_vars.push_back(NegationOf(var));
      model->Add(GreaterOrEqual(max_var, var));
    }

    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    MinPropagator* constraint =
        new MinPropagator(negated_vars, NegationOf(max_var), integer_trail);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

// Creates an integer variable equal to the minimum of other integer variables.
inline std::function<IntegerVariable(Model*)> NewMin(
    const std::vector<IntegerVariable>& vars) {
  return [=](Model* model) {
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();

    // The trival bounds will be propagated correctly at level zero.
    IntegerVariable min_var = integer_trail->AddIntegerVariable();
    model->Add(IsEqualToMinOf(min_var, vars));
    return min_var;
  };
}

// Creates an IntegerVariable equal to the maximum of a set of IntegerVariables.
inline std::function<IntegerVariable(Model*)> NewMax(
    const std::vector<IntegerVariable>& vars) {
  return [=](Model* model) {
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();

    // The trival bounds will be propagated correctly at level zero.
    IntegerVariable max_var = integer_trail->AddIntegerVariable();
    model->Add(IsEqualToMaxOf(max_var, vars));
    return max_var;
  };
}

// Expresses the fact that an existing integer variable is equal to one of
// the given values, each selected by a given literal.
inline std::function<void(Model*)> IsOneOf(IntegerVariable var,
                                           const std::vector<Literal>& selectors,
                                           const std::vector<IntegerValue>& values) {
  return [=](Model* model) {
    // TODO(user): Add the exactly one constaint here?
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    IsOneOfPropagator* constraint =
        new IsOneOfPropagator(var, selectors, values, integer_trail);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_INTEGER_EXPR_H_
