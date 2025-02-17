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

#ifndef OR_TOOLS_SAT_CP_CONSTRAINTS_H_
#define OR_TOOLS_SAT_CP_CONSTRAINTS_H_

#include <cstdint>
#include <functional>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

// Propagate the fact that a XOR of literals is equal to the given value.
// The complexity is in O(n).
//
// TODO(user): By using a two watcher mechanism, we can propagate this a lot
// faster.
class BooleanXorPropagator : public PropagatorInterface {
 public:
  BooleanXorPropagator(const std::vector<Literal>& literals, bool value,
                       Trail* trail, IntegerTrail* integer_trail)
      : literals_(literals),
        value_(value),
        trail_(trail),
        integer_trail_(integer_trail) {}

  // This type is neither copyable nor movable.
  BooleanXorPropagator(const BooleanXorPropagator&) = delete;
  BooleanXorPropagator& operator=(const BooleanXorPropagator&) = delete;

  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  const std::vector<Literal> literals_;
  const bool value_;
  std::vector<Literal> literal_reason_;
  Trail* trail_;
  IntegerTrail* integer_trail_;
};

// If we have:
//  - selectors[i] =>  (target_var >= vars[i] + offset[i])
//  - and we known that at least one selectors[i] must be true
// then we can propagate the fact that if no selectors is chosen yet, the lower
// bound of target_var is greater than the min of the still possible
// alternatives.
//
// This constraint take care of this case when no selectors[i] is chosen yet.
//
// This constraint support duplicate selectors.
class GreaterThanAtLeastOneOfPropagator : public PropagatorInterface,
                                          public LazyReasonInterface {
 public:
  GreaterThanAtLeastOneOfPropagator(IntegerVariable target_var,
                                    absl::Span<const AffineExpression> exprs,
                                    absl::Span<const Literal> selectors,
                                    absl::Span<const Literal> enforcements,
                                    Model* model);

  // This type is neither copyable nor movable.
  GreaterThanAtLeastOneOfPropagator(const GreaterThanAtLeastOneOfPropagator&) =
      delete;
  GreaterThanAtLeastOneOfPropagator& operator=(
      const GreaterThanAtLeastOneOfPropagator&) = delete;

  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

  // For LazyReasonInterface.
  void Explain(int id, IntegerValue propagation_slack,
               IntegerVariable var_to_explain, int trail_index,
               std::vector<Literal>* literals_reason,
               std::vector<int>* trail_indices_reason) final;

 private:
  const IntegerVariable target_var_;
  const std::vector<Literal> enforcements_;

  // Non-const as we swap elements around.
  std::vector<Literal> selectors_;
  std::vector<AffineExpression> exprs_;

  Trail* trail_;
  IntegerTrail* integer_trail_;
};

// ============================================================================
// Model based functions.
// ============================================================================

inline std::vector<IntegerValue> ToIntegerValueVector(
    absl::Span<const int64_t> input) {
  std::vector<IntegerValue> result(input.size());
  for (int i = 0; i < input.size(); ++i) {
    result[i] = IntegerValue(input[i]);
  }
  return result;
}

// Enforces the XOR of a set of literals to be equal to the given value.
inline std::function<void(Model*)> LiteralXorIs(
    const std::vector<Literal>& literals, bool value) {
  return [=](Model* model) {
    Trail* trail = model->GetOrCreate<Trail>();
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    BooleanXorPropagator* constraint =
        new BooleanXorPropagator(literals, value, trail, integer_trail);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

inline std::function<void(Model*)> GreaterThanAtLeastOneOf(
    IntegerVariable target_var, const absl::Span<const IntegerVariable> vars,
    const absl::Span<const IntegerValue> offsets,
    const absl::Span<const Literal> selectors,
    const absl::Span<const Literal> enforcements) {
  return [=](Model* model) {
    std::vector<AffineExpression> exprs;
    for (int i = 0; i < vars.size(); ++i) {
      exprs.push_back(AffineExpression(vars[i], 1, offsets[i]));
    }
    GreaterThanAtLeastOneOfPropagator* constraint =
        new GreaterThanAtLeastOneOfPropagator(target_var, exprs, selectors,
                                              enforcements, model);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

// The target variable is equal to exactly one of the candidate variable. The
// equality is controlled by the given "selector" literals.
//
// Note(user): This only propagate from the min/max of still possible candidates
// to the min/max of the target variable. The full constraint also requires
// to deal with the case when one of the literal is true.
//
// Note(user): If there is just one or two candidates, this doesn't add
// anything.
inline std::function<void(Model*)> PartialIsOneOfVar(
    IntegerVariable target_var, absl::Span<const IntegerVariable> vars,
    absl::Span<const Literal> selectors) {
  CHECK_EQ(vars.size(), selectors.size());
  return [=,
          selectors = std::vector<Literal>(selectors.begin(), selectors.end()),
          vars = std::vector<IntegerVariable>(vars.begin(), vars.end())](
             Model* model) {
    const std::vector<IntegerValue> offsets(vars.size(), IntegerValue(0));
    if (vars.size() > 2) {
      // Propagate the min.
      model->Add(
          GreaterThanAtLeastOneOf(target_var, vars, offsets, selectors, {}));
    }
    if (vars.size() > 2) {
      // Propagate the max.
      model->Add(GreaterThanAtLeastOneOf(
          NegationOf(target_var), NegationOf(vars), offsets, selectors, {}));
    }
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_CONSTRAINTS_H_
