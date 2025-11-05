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

#ifndef ORTOOLS_SAT_ENFORCEMENT_HELPER_H_
#define ORTOOLS_SAT_ENFORCEMENT_HELPER_H_

#include <vector>

#include "absl/base/attributes.h"
#include "absl/types/span.h"
#include "ortools/sat/enforcement.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

// This is meant as an helper to deal with enforcement for any integer based
// constraint. It wraps some IntegerTrail functions while making sure the
// enforcement literals are properly added to the propagation reason.
class EnforcementHelper {
 public:
  explicit EnforcementHelper(Model* model);

  // Calls `Register` with a callback calling
  // `watcher->CallOnNextPropagate(literal_watcher_id)` if a propagation might
  // be possible.
  EnforcementId Register(absl::Span<const Literal> enforcement_literals,
                         GenericLiteralWatcher* watcher,
                         int literal_watcher_id);

  // Add the enforcement reason to the given vector.
  void AddEnforcementReason(EnforcementId id,
                            std::vector<Literal>* reason) const {
    enforcement_propagator_.AddEnforcementReason(id, reason);
  }

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
    return enforcement_propagator_.Status(id);
  }

  // Returns the enforcement literals of the given id.
  absl::Span<const Literal> GetEnforcementLiterals(EnforcementId id) const {
    return enforcement_propagator_.GetEnforcementLiterals(id);
  }

 private:
  EnforcementPropagator& enforcement_propagator_;
  const VariablesAssignment& assignment_;
  IntegerTrail* integer_trail_;
  std::vector<Literal> temp_reason_;
  std::vector<IntegerLiteral> temp_integer_reason_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_ENFORCEMENT_HELPER_H_
