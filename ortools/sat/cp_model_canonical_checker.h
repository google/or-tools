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

#ifndef ORTOOLS_SAT_CP_MODEL_CANONICAL_CHECKER_H_
#define ORTOOLS_SAT_CP_MODEL_CANONICAL_CHECKER_H_

#include <functional>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

// Compares two LinearExpressionProtos lexicographically by (vars, coeffs,
// offset). Used as the canonical expression sort order in kLinMax, kIntProd,
// and kAllDiff.
bool LinearExpressionProtoLessThan(const LinearExpressionProto& a,
                                   const LinearExpressionProto& b);

// Debug / test helper that checks whether a CP-SAT constraint is in canonical
// form given variable domains and an optional affine-representative predicate.
class CanonicalizationChecker {
 public:
  struct Options {
    // True in LRAT proof modes that intentionally skip kBoolOr / kBoolAnd
    // canonicalization so clauses stay aligned with emitted LRAT proof steps.
    // TODO(user): see if we can apply the missing canonicalizations to
    // PresolvePureSatProblem() and remove this option.
    bool lrat_mode = false;

    // Optional hook for live mid-presolve affine equivalence state.
    std::function<bool(int var)> var_is_representative;
  };

  explicit CanonicalizationChecker(const std::vector<Domain>* domains,
                                   Options options = DefaultOptions());

  bool ConstraintIsCanonical(const ConstraintProto& ct,
                             bool check_sorted = true) const;

 private:
  static Options DefaultOptions() { return Options{}; }

  bool IsFixed(int ref) const;
  bool LiteralIsFalse(int lit) const;
  bool IsRepresentative(int ref) const;
  bool LiteralsAreCanonical(absl::Span<const int> lits, bool check_enforcements,
                            bool check_sorted) const;
  bool LinearExpressionIsCanonical(const LinearExpressionProto& expr,
                                   bool check_sorted) const;

  const std::vector<Domain>* domains_ = nullptr;
  Options options_;
  mutable absl::flat_hash_set<int> tmp_enforcements_;
  mutable absl::flat_hash_set<int> tmp_seen_vars_;
};

// Standalone post-presolve / test check (`check_sorted = false` by default
// because post-presolve variable remapping/permutation does not preserve sorted
// variable indices).
bool ModelProtoIsCanonical(const CpModelProto& model, bool lrat_mode = false,
                           bool check_sorted = false);

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_CP_MODEL_CANONICAL_CHECKER_H_
