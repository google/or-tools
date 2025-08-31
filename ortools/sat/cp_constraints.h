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
#include <ostream>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

DEFINE_STRONG_INDEX_TYPE(EnforcementId);

// An enforced constraint can be in one of these 4 states.
// Note that we rely on the integer encoding to take 2 bits for optimization.
enum class EnforcementStatus {
  // One enforcement literal is false.
  IS_FALSE = 0,
  // More than two literals are unassigned.
  CANNOT_PROPAGATE = 1,
  // All enforcement literals are true but one.
  CAN_PROPAGATE_ENFORCEMENT = 2,
  // All enforcement literals are true.
  IS_ENFORCED = 3,
};

std::ostream& operator<<(std::ostream& os, const EnforcementStatus& e);

// This is meant as an helper to deal with enforcement for any constraint.
class EnforcementPropagator : public SatPropagator {
 public:
  explicit EnforcementPropagator(Model* model);

  // SatPropagator interface.
  bool Propagate(Trail* trail) final;
  void Untrail(const Trail& trail, int trail_index) final;

  // Adds a new constraint to the class and register a callback that will
  // be called on status change. Note that we also call the callback with the
  // initial status if different from CANNOT_PROPAGATE when added.
  //
  // It is better to not call this for empty enforcement list, but you can. A
  // negative id means the level zero status will never change, and only the
  // first call to callback() should be necessary, we don't save it.
  EnforcementId Register(
      absl::Span<const Literal> enforcement,
      std::function<void(EnforcementId, EnforcementStatus)> callback = nullptr);

  // Calls `Register` with a callback calling
  // `watcher->CallOnNextPropagate(literal_watcher_id)` if a propagation might
  // be possible.
  EnforcementId Register(absl::Span<const Literal> enforcement_literals,
                         GenericLiteralWatcher* watcher,
                         int literal_watcher_id);

  // Add the enforcement reason to the given vector.
  void AddEnforcementReason(EnforcementId id,
                            std::vector<Literal>* reason) const;

  // Try to propagate when the enforced constraint is not satisfiable.
  // This is currently in O(enforcement_size).
  ABSL_MUST_USE_RESULT bool PropagateWhenFalse(
      EnforcementId id, absl::Span<const Literal> literal_reason,
      absl::Span<const IntegerLiteral> integer_reason);

  ABSL_MUST_USE_RESULT bool Enqueue(
      EnforcementId id, IntegerLiteral i_lit,
      absl::Span<const Literal> literal_reason,
      absl::Span<const IntegerLiteral> integer_reason);

  ABSL_MUST_USE_RESULT bool SafeEnqueue(
      EnforcementId id, IntegerLiteral i_lit,
      absl::Span<const IntegerLiteral> integer_reason);

  ABSL_MUST_USE_RESULT bool ConditionalEnqueue(
      EnforcementId id, Literal lit, IntegerLiteral i_lit,
      absl::Span<const Literal> literal_reason,
      absl::Span<const IntegerLiteral> integer_reason);

  void EnqueueLiteral(EnforcementId id, Literal literal,
                      absl::Span<const Literal> literal_reason,
                      absl::Span<const IntegerLiteral> integer_reason);

  bool ReportConflict(EnforcementId id,
                      absl::Span<const IntegerLiteral> integer_reason) {
    return ReportConflict(id, /*literal_reason=*/{}, integer_reason);
  }

  bool ReportConflict(EnforcementId id,
                      absl::Span<const Literal> literal_reason,
                      absl::Span<const IntegerLiteral> integer_reason);

  EnforcementStatus Status(EnforcementId id) const {
    if (id < 0) return EnforcementStatus::IS_ENFORCED;
    return statuses_[id];
  }

  // Recompute the status from the current assignment.
  // This should only used in DCHECK().
  EnforcementStatus DebugStatus(EnforcementId id);

  // Returns the enforcement literals of the given id.
  absl::Span<const Literal> GetEnforcementLiterals(EnforcementId id) const {
    if (id < 0) return {};
    return GetSpan(id);
  }

 private:
  absl::Span<Literal> GetSpan(EnforcementId id);
  absl::Span<const Literal> GetSpan(EnforcementId id) const;
  void ChangeStatus(EnforcementId id, EnforcementStatus new_status);

  // Returns kNoLiteralIndex if nothing need to change or a new literal to
  // watch. This also calls the registered callback.
  LiteralIndex ProcessIdOnTrue(Literal watched, EnforcementId id);

  // External classes.
  const Trail& trail_;
  const VariablesAssignment& assignment_;
  IntegerTrail* integer_trail_;
  RevIntRepository* rev_int_repository_;

  // All enforcement will be copied there, and we will create Span out of this.
  // Note that we don't store the span so that we are not invalidated on buffer_
  // resizing.
  util_intops::StrongVector<EnforcementId, int> starts_;
  std::vector<Literal> buffer_;

  util_intops::StrongVector<EnforcementId, EnforcementStatus> statuses_;
  util_intops::StrongVector<
      EnforcementId, std::function<void(EnforcementId, EnforcementStatus)>>
      callbacks_;

  // Used to restore status and call callback on untrail.
  std::vector<std::pair<EnforcementId, EnforcementStatus>> untrail_stack_;
  int rev_stack_size_ = 0;
  int64_t rev_stamp_ = 0;

  // We use a two watcher scheme.
  util_intops::StrongVector<LiteralIndex, absl::InlinedVector<EnforcementId, 6>>
      watcher_;

  std::vector<Literal> temp_literals_;
  std::vector<Literal> temp_reason_;
  std::vector<IntegerLiteral> temp_integer_reason_;

  std::vector<EnforcementId> ids_to_fix_until_next_root_level_;
};

// Propagate the fact that a XOR of literals is equal to the given value.
// The complexity is in O(n).
//
// TODO(user): By using a two watcher mechanism, we can propagate this a lot
// faster.
class BooleanXorPropagator : public PropagatorInterface {
 public:
  BooleanXorPropagator(absl::Span<const Literal> enforcement_literals,
                       const std::vector<Literal>& literals, bool value,
                       Model* model);

  // This type is neither copyable nor movable.
  BooleanXorPropagator(const BooleanXorPropagator&) = delete;
  BooleanXorPropagator& operator=(const BooleanXorPropagator&) = delete;

  bool Propagate() final;

 private:
  int RegisterWith(GenericLiteralWatcher* watcher);

  const std::vector<Literal> literals_;
  const bool value_;
  std::vector<Literal> literal_reason_;
  const Trail& trail_;
  const IntegerTrail& integer_trail_;
  EnforcementPropagator& enforcement_propagator_;
  EnforcementId enforcement_id_;
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
    const std::vector<Literal>& enforcement_literals,
    const std::vector<Literal>& literals, bool value) {
  return [=](Model* model) {
    model->TakeOwnership(
        new BooleanXorPropagator(enforcement_literals, literals, value, model));
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
