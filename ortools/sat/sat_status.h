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

#ifndef ORTOOLS_SAT_SAT_STATUS_H_
#define ORTOOLS_SAT_SAT_STATUS_H_

#include <cstdint>

namespace operations_research {
namespace sat {

// Tag type representing root-level infeasibility (UNSAT). Implicitly converts
// to the UNSAT state of `FeasibilityStatus` and `ChangeStatus`.
class [[nodiscard]] UnsatStatus {
 public:
  friend constexpr bool operator==(UnsatStatus, UnsatStatus) = default;
};
inline constexpr UnsatStatus kUnsat{};

// Returned by root-level / presolve operations that may prove infeasibility
// (`kUnsat`) or finish without proving infeasibility (`kOk`).
class [[nodiscard]] FeasibilityStatus {
 public:
  constexpr FeasibilityStatus(UnsatStatus) : is_unsat_(true) {}
  explicit constexpr FeasibilityStatus(bool might_be_feasible)
      : is_unsat_(!might_be_feasible) {}
  constexpr bool ok() const { return !is_unsat_; }
  constexpr bool unsat() const { return is_unsat_; }
  friend constexpr bool operator==(FeasibilityStatus,
                                   FeasibilityStatus) = default;

 private:
  bool is_unsat_;
};
inline constexpr FeasibilityStatus kOk{true};

// Returned by helpers that cannot prove UNSAT and report whether their target
// was modified. Intentionally not `[[nodiscard]]` because callers frequently
// invoke in-place normalization helpers without inspecting whether changes
// occurred.
class ModificationStatus {
 public:
  explicit constexpr ModificationStatus(bool modified) : modified_(modified) {}
  constexpr bool modified() const { return modified_; }
  constexpr ModificationStatus& operator|=(ModificationStatus other) {
    modified_ |= other.modified_;
    return *this;
  }
  friend constexpr bool operator==(ModificationStatus,
                                   ModificationStatus) = default;

 private:
  bool modified_;
};
inline constexpr ModificationStatus kUnchanged{false};
inline constexpr ModificationStatus kModified{true};

// Returned by root-level / presolve operations that maintain invariants
// internally, may prove infeasibility (`kUnsat`), and report whether their
// target was modified (`kModified` vs `kUnchanged`).
class [[nodiscard]] ChangeStatus {
 public:
  constexpr ChangeStatus(UnsatStatus) : state_(State::kUnsat) {}
  constexpr ChangeStatus(ModificationStatus status)
      : ChangeStatus(status.modified()) {}
  explicit constexpr ChangeStatus(FeasibilityStatus status)
      : state_(status.unsat() ? State::kUnsat : State::kUnchanged) {}
  explicit constexpr ChangeStatus(bool modified)
      : state_(modified ? State::kModified : State::kUnchanged) {}
  constexpr bool unsat() const { return state_ == State::kUnsat; }
  constexpr bool modified() const { return state_ == State::kModified; }
  constexpr ChangeStatus& operator|=(ModificationStatus status) {
    if (status.modified() && state_ == State::kUnchanged) {
      state_ = State::kModified;
    }
    return *this;
  }
  explicit constexpr operator FeasibilityStatus() const {
    return unsat() ? FeasibilityStatus(kUnsat) : kOk;
  }
  friend constexpr bool operator==(ChangeStatus, ChangeStatus) = default;

 private:
  enum class State : uint8_t {
    kUnsat,
    kUnchanged,
    kModified,
  };
  State state_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_SAT_STATUS_H_
