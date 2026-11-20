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

#include <cstdint>
#include <functional>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

// Tag type representing infeasibility (UNSAT). Implicitly converts to the
// UNSAT state of both `FeasibilityStatus` and `CanonicalizationStatus`.
class [[nodiscard]] UnsatStatus {
 public:
  friend constexpr bool operator==(UnsatStatus, UnsatStatus) = default;
};
inline constexpr UnsatStatus kUnsat{};

// TODO(user): Move this somewhere else where it can easily be used
// throughout the codebase, and use it to replace `bool` return types meaning
// "unsat".
class [[nodiscard]] FeasibilityStatus {
 public:
  constexpr FeasibilityStatus(UnsatStatus) : is_unsat_(true) {}
  explicit constexpr FeasibilityStatus(bool might_be_feasible)
      : is_unsat_(!might_be_feasible) {}
  constexpr bool unsat() const { return is_unsat_; }
  constexpr bool unknown() const { return !is_unsat_; }
  friend constexpr bool operator==(FeasibilityStatus,
                                   FeasibilityStatus) = default;

 private:
  bool is_unsat_;
};
inline constexpr FeasibilityStatus kUnknown{true};

// Returned by single-constraint presolve helpers that may either detect UNSAT
// or mutate constraint `c` in place without updating its canonicality or
// constraint-variable graph:
// - `kUnsat`: model infeasibility was detected; caller must abort immediately.
// - `kCanonical`: no infeasibility was detected, and `c` is untouched, cleared,
//   already canonicalized, or already marked for lazy canonicalization.
// - `kNeedsCanonicalization`: no infeasibility was detected, and caller must
//   mark or canonicalize `c`.
class [[nodiscard]] CanonicalizationStatus {
 public:
  constexpr CanonicalizationStatus(UnsatStatus) : state_(State::kUnsat) {}
  explicit constexpr CanonicalizationStatus(FeasibilityStatus status)
      : state_(status.unsat() ? State::kUnsat : State::kCanonical) {}
  explicit constexpr CanonicalizationStatus(bool needs_canonicalization)
      : state_(needs_canonicalization ? State::kNeedsCanonicalization
                                      : State::kCanonical) {}
  constexpr bool unsat() const { return state_ == State::kUnsat; }
  constexpr bool needs_canonicalization() const {
    return state_ == State::kNeedsCanonicalization;
  }
  explicit constexpr operator FeasibilityStatus() const {
    DCHECK(!needs_canonicalization());
    return unsat() ? FeasibilityStatus(kUnsat) : kUnknown;
  }
  friend constexpr bool operator==(CanonicalizationStatus,
                                   CanonicalizationStatus) = default;

 private:
  enum class State : uint8_t {
    kUnsat,
    kCanonical,
    kNeedsCanonicalization,
  };
  State state_;
};
inline constexpr CanonicalizationStatus kCanonical{false};
inline constexpr CanonicalizationStatus kNeedsCanonicalization{true};

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
