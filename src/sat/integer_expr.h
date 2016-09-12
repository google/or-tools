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

// A really basic implementation of a sum of integer variables.
// The complexity is in O(num_variables) at each propagation.
//
// TODO(user): Propagate all the bounds.
// TODO(user): If one has many such constraint, it will be more efficient to
// propagate all of them at once rather than doing it one at the time.
class IntegerSum : public PropagatorInterface {
 public:
  IntegerSum(const std::vector<IntegerVariable>& vars,
             const std::vector<int>& coefficients, IntegerVariable sum,
             IntegerTrail* integer_trail);

  // Currently we only propagates the directions:
  // * vars lower-bound -> sum lower-bound.
  // * for all vars i,
  //   vars lower-bound (excluding i) + sum upper_bound -> i upper-bound.
  bool Propagate(Trail* trail) final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  std::vector<IntegerVariable> vars_;
  std::vector<int> coeffs_;
  IntegerVariable sum_;
  IntegerTrail* integer_trail_;

  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> integer_reason_;

  DISALLOW_COPY_AND_ASSIGN(IntegerSum);
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

// Model-based function to create an IntegerVariable that corresponds to the
// given weighted sum of other IntegerVariables.
inline std::function<IntegerVariable(Model*)> NewWeightedSum(
    const std::vector<int>& coefficients, const std::vector<IntegerVariable>& vars) {
  return [=](Model* model) {
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();

    // The trival bounds will be propagated correctly at level zero.
    IntegerVariable sum = integer_trail->AddIntegerVariable();
    IntegerSum* constraint =
        new IntegerSum(vars, coefficients, sum, integer_trail);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
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
