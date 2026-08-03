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

// Common types and constants used by the Linear Programming solver.

#ifndef ORTOOLS_LP_DATA_LP_TYPES_H_
#define ORTOOLS_LP_DATA_LP_TYPES_H_

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "ortools/base/strong_vector.h"
#include "ortools/glop/status.h"
#include "ortools/util/bitset.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

// We use typedefs as much as possible to later permit the usage of
// types such as quad-doubles or rationals.

namespace operations_research {
namespace glop {

// This type is defined to avoid cast issues during index conversions,
// e.g. converting ColIndex into RowIndex.
// All types should use 'Index' instead of int32_t.
typedef int32_t Index;

// ColIndex is the type for integers representing column/variable indices.
// int32s are enough for handling even the largest problems.
DEFINE_STRONG_INDEX_TYPE(ColIndex);

// RowIndex is the type for integers representing row/constraint indices.
// int32s are enough for handling even the largest problems.
DEFINE_STRONG_INDEX_TYPE(RowIndex);

// Get the ColIndex corresponding to the column # row.
inline ColIndex RowToColIndex(RowIndex row) { return ColIndex(row.value()); }

// Get the RowIndex corresponding to the row # col.
inline RowIndex ColToRowIndex(ColIndex col) { return RowIndex(col.value()); }

// Get the integer index corresponding to the col.
inline Index ColToIntIndex(ColIndex col) { return col.value(); }

// Get the integer index corresponding to the row.
inline Index RowToIntIndex(RowIndex row) { return row.value(); }

// EntryIndex is the type for integers representing entry indices.
// An entry in a sparse matrix is a pair (row, value) for a given known column.
// See classes SparseColumn and SparseMatrix.
#if defined(__ANDROID__)
DEFINE_STRONG_INDEX_TYPE(EntryIndex);
#else
DEFINE_STRONG_INT64_TYPE(EntryIndex);
#endif

static inline double ToDouble(double f) { return f; }

// The type Fractional denotes the type of numbers on which the computations are
// performed. This is defined as double here, but it could as well be float,
// DoubleDouble, QuadDouble, or infinite-precision rationals.
// Floating-point representations are binary fractional numbers, thus the name.
// (See http://en.wikipedia.org/wiki/Fraction_(mathematics) .)
typedef double Fractional;

// Range max for type Fractional. DBL_MAX for double for example.
constexpr Fractional kRangeMax = std::numeric_limits<Fractional>::max();

// Infinity for type Fractional.
constexpr Fractional kInfinity = std::numeric_limits<Fractional>::infinity();

// Epsilon for type Fractional, i.e. the smallest e such that 1.0 + e != 1.0 .
constexpr Fractional kEpsilon = std::numeric_limits<Fractional>::epsilon();

// Returns true if the given value is finite, that means for a double:
// not a NaN and not +/- infinity.
inline bool IsFinite(Fractional value) {
  return value > -kInfinity && value < kInfinity;
}

// Constants to represent invalid row or column index.
// It is important that their values be the same because during transposition,
// one needs to be converted into the other.
constexpr RowIndex kInvalidRow(-1);
constexpr ColIndex kInvalidCol(-1);

// Different statuses for a given problem.
enum class ProblemStatus : int8_t {
  // The problem has been solved to optimality. Both the primal and dual have
  // a feasible solution.
  OPTIMAL,

  // The problem has been proven primal-infeasible. Note that the problem is not
  // necessarily DUAL_UNBOUNDED (See Chvatal p.60). The solver does not have a
  // dual unbounded ray in this case.
  PRIMAL_INFEASIBLE,

  // The problem has been proven dual-infeasible. Note that the problem is not
  // necessarily PRIMAL_UNBOUNDED (See Chvatal p.60). The solver does
  // note have a primal unbounded ray in this case,
  DUAL_INFEASIBLE,

  // The problem is either INFEASIBLE or UNBOUNDED (this applies to both the
  // primal and dual algorithms). This status is only returned by the presolve
  // step and means that a primal or dual unbounded ray was found during
  // presolve. Note that because some presolve techniques assume that a feasible
  // solution exists to simplify the problem further, it is difficult to
  // distinguish between infeasibility and unboundedness.
  //
  // If a client needs to distinguish, it is possible to run the primal
  // algorithm on the same problem with a 0 objective function to know if the
  // problem was PRIMAL_INFEASIBLE.
  INFEASIBLE_OR_UNBOUNDED,

  // The problem has been proven feasible and unbounded. That means that the
  // problem is DUAL_INFEASIBLE and that the solver has a primal unbounded ray.
  PRIMAL_UNBOUNDED,

  // The problem has been proven dual-feasible and dual-unbounded. That means
  // the problem is PRIMAL_INFEASIBLE and that the solver has a dual unbounded
  // ray to prove it.
  DUAL_UNBOUNDED,

  // All the statuses below correspond to a case where the solver was
  // interrupted. This can happen because of a timeout, an iteration limit or an
  // error.

  // The solver didn't had a chance to prove anything.
  INIT,

  // The problem has been proven primal-feasible but may still be
  // PRIMAL_UNBOUNDED.
  PRIMAL_FEASIBLE,

  // The problem has been proven dual-feasible, but may still be DUAL_UNBOUNDED.
  // That means that if the primal is feasible, then it has a finite optimal
  // solution.
  DUAL_FEASIBLE,

  // An error occurred during the solving process.
  ABNORMAL,

  // The input problem was invalid (see LinearProgram.IsValid()).
  INVALID_PROBLEM,

  // The problem was solved to a feasible status, but the solution checker found
  // the primal and/or dual infeasibilities too important for the specified
  // parameters.
  IMPRECISE,
};

// Returns the string representation of the ProblemStatus enum.
std::string GetProblemStatusString(ProblemStatus problem_status);

inline std::ostream& operator<<(std::ostream& os, ProblemStatus status) {
  os << GetProblemStatusString(status);
  return os;
}

// Different causes of early interruption of the solve (not including errors).
//
// See TimeLimitStateToCause().
enum class InterruptionCause : int8_t {
  // The solve was interrupted with an elapsed time limit (could be
  // deterministic time or wall time).
  //
  // This corresponds to TimeLimit::LimitReached() being true because of the
  // time. See kExternalBoolean for other case.
  kTimeLimit,

  // The solve was interrupted because of an external interruption.
  //
  // This corresponds to TimeLimit::LimitReached() being true because of the
  // boolean passed to TimeLimit::RegisterExternalBooleanAsLimit().
  kExternal,

  // The solve was interrupted due to the number of iterations.
  kIterationLimit,

  // The objective limit has been reached.
  kObjectiveLimit,
};

// Returns the string representation of the InterruptionCause enum.
absl::string_view GetInterruptionCauseString(InterruptionCause cause);

inline std::ostream& operator<<(std::ostream& os, InterruptionCause cause) {
  os << GetInterruptionCauseString(cause);
  return os;
}

// Converts the state of the TimeLimit to a cause of interruption. Returns
// std::nullopt when called with TimeLimit::State::kRunning.
inline std::optional<InterruptionCause> TimeLimitStateToCause(
    TimeLimit::State state) {
  switch (state) {
    case TimeLimit::State::kRunning:
      return std::nullopt;
    case TimeLimit::State::kExternalBoolean:
    case TimeLimit::State::kSecondaryExternalBoolean:
      return InterruptionCause::kExternal;
    case TimeLimit::State::kDeterministicTime:
    case TimeLimit::State::kTime:
      return InterruptionCause::kTimeLimit;
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid TimeLimit::State " << static_cast<int>(state);
  return InterruptionCause::kTimeLimit;
}

// Computes the state of the TimeLimit and convert it to a cause of
// interruption. Returns std::nullopt when called with
// TimeLimit::State::kRunning.
inline std::optional<InterruptionCause> TimeLimitStateToCause(
    TimeLimit& time_limit) {
  return TimeLimitStateToCause(time_limit.CurrentState());
}

// Feasibility status of the problem.
//
// This is not a final status, this different from ProblemStatus/SolveStatus:
//
// * ProblemStatus::PRIMAL_FEASIBLE (or SolveStatus::PrimalFeasible) means that
//   the solver was interrupted (time-limit, number of iterations...).
//
// * FeasibilityStatus::kPrimal means the problem is currently known to be
//   feasible but the solver is not done yet.
enum class FeasibilityStatus { kPrimal, kDual };

absl::string_view GetFeasibilityStatusString(FeasibilityStatus feasibility);
inline std::ostream& operator<<(std::ostream& out,
                                const FeasibilityStatus feasibility) {
  out << GetFeasibilityStatusString(feasibility);
  return out;
}

// Different causes of a ProblemStatus::ABNORMAL.
//
// This enum should not be exhaustively `switch` on in user code! Instead
// functions declared here like GetAbnormalityCauseString() or operator<< should
// be used. We use a enum to keep the memory footprint small instead of using
// strings and help locate errors in the code.
//
// The names are chosen to be kSrcFileErrorName, where SrcFile is the CamelCase
// of the source name and ErrorName is an arbitrary name that describes the
// error.
//
// For functions that can either succeed of fail with an abnormality, the type
// AbnormalityStatus can be used.
enum class AbnormalityCause : int8_t {
  // LU factorization error: failed to generate the rank one update matrix.
  kLuFactorizationDegenerateRankOneUpdate,

  // LU factorization error: matrix is not square.
  kLuFactorizationNotSquare,

  // LU factorization error: too small pivot number.
  kLuFactorizationPivotTooSmall,

  // LU factorization/markowitz error: too small pivot number.
  kLuFactorizationMarkowitzPivotTooSmall,

  // The solution produced is not consistent.
  kLpSolverInconsistentSolution,

  // The phase-I problem is unbounded.
  kRevisedSimplexUnboundedFeasibilityProblem,

  // Too high condition number; see initial_condition_number_threshold.
  kRevisedSimplexConditionNumberTooHigh,

  // Overflow in step_length.
  kRevisedSimplexStepLengthOverflow,

  // Pivot value too small.
  kRevisedSimplexPivotTooSmall,

  // No unmarked entry in a row or column.
  kSingletonPreprocessorNoUnmarkedEntry,

  // Overflow in computations in DoubletonEqualityRowPreprocessor.
  kDoubletonEqualityRowPreprocessorOverflow,

  // See enum documentation, no exhaustive switch should be used.
  kNotForUseWithExhaustiveSwitchStatements,
};

absl::string_view GetAbnormalityCauseString(AbnormalityCause cause);
inline std::ostream& operator<<(std::ostream& out,
                                const AbnormalityCause feasibility) {
  out << GetAbnormalityCauseString(feasibility);
  return out;
}

// Wrapper around std::optional<AbnormalityCause> that adds:
// * [[nodiscard]], to prevent ignore errors,
// * an operator<< for std::ostream,
// * a convenient ok() shortcut.
//
// See GLOP_RETURN_IF_ABNORMAL() to propagate failures.
// See lp_types_testing.h for tests.
struct [[nodiscard]] AbnormalityStatus {
  // No error.
  //
  // Prefer using OkAbnormalityStatus().
  explicit AbnormalityStatus() = default;

  // An error happened.
  explicit AbnormalityStatus(const AbnormalityCause cause) : cause(cause) {}

  // Returns true when `cause` is unset (no error occurred).
  bool ok() const { return !cause.has_value(); }

  // Unset means no error.
  std::optional<AbnormalityCause> cause;
};

// Prints either OK or ABNORMAL(Xxx).
std::ostream& operator<<(std::ostream& out, const AbnormalityStatus& status);

// Returns an OK AbnormalityStatus.
inline AbnormalityStatus OkAbnormalityStatus() { return AbnormalityStatus{}; }

// Evaluates expr once, which should evaluate to an AbnormalityStatus and
// returns when AbnormalityStatus.ok() is false.
//
// It returns a value that implicitly converts to:
// * either AbnormalityStatus,
// * or SolveStatus built from AbnormalSolveStatus.
//
// So this macro can be used in functions with AbnormalityStatus or SolveStatus
// return types.
#define GLOP_RETURN_IF_ABNORMAL(expr)                                   \
  if (const AbnormalityStatus status = (expr); !status.ok()) {          \
    return internal::GlopReturnIfAbnormalError{.cause = *status.cause}; \
  }

// Status of a solve.
//
// It pairs ProblemStatus and InterruptionCause/AbnormalityCause, making sure we
// have a cause for cases where the status due to an interruption (see
// ProblemStatus comments). To do so there is one `struct Xxx` for each
// status in ProblemStatus. Each of these types have:
// - a `status` field with the corresponding ProblemStatus,
// - a optional `cause` field for statuses that have a cause
//
// The XxxSolveStatus() factories should be used to simplify call sites.
//
// The problem_status() and cause() can be used for users that don't want to use
// the std::variant type.
//
// See FeasibilityStatus.
struct [[nodiscard]] SolveStatus {
  struct Optimal {
    static constexpr ProblemStatus status = ProblemStatus::OPTIMAL;
    friend bool operator==(const Optimal&, const Optimal&) = default;
  };
  struct PrimalInfeasible {
    static constexpr ProblemStatus status = ProblemStatus::PRIMAL_INFEASIBLE;
    friend bool operator==(const PrimalInfeasible&,
                           const PrimalInfeasible&) = default;
  };
  struct DualInfeasible {
    static constexpr ProblemStatus status = ProblemStatus::DUAL_INFEASIBLE;
    friend bool operator==(const DualInfeasible&,
                           const DualInfeasible&) = default;
  };
  struct InfeasibleOrUnbounded {
    static constexpr ProblemStatus status =
        ProblemStatus::INFEASIBLE_OR_UNBOUNDED;
    friend bool operator==(const InfeasibleOrUnbounded&,
                           const InfeasibleOrUnbounded&) = default;
  };
  struct PrimalUnbounded {
    static constexpr ProblemStatus status = ProblemStatus::PRIMAL_UNBOUNDED;
    friend bool operator==(const PrimalUnbounded&,
                           const PrimalUnbounded&) = default;
  };
  struct DualUnbounded {
    static constexpr ProblemStatus status = ProblemStatus::DUAL_UNBOUNDED;
    friend bool operator==(const DualUnbounded&,
                           const DualUnbounded&) = default;
  };
  struct Init {
    static constexpr ProblemStatus status = ProblemStatus::INIT;
    InterruptionCause cause ABSL_REQUIRE_EXPLICIT_INIT;
    friend bool operator==(const Init&, const Init&) = default;
  };
  struct PrimalFeasible {
    static constexpr ProblemStatus status = ProblemStatus::PRIMAL_FEASIBLE;
    InterruptionCause cause ABSL_REQUIRE_EXPLICIT_INIT;
    friend bool operator==(const PrimalFeasible&,
                           const PrimalFeasible&) = default;
  };
  struct DualFeasible {
    static constexpr ProblemStatus status = ProblemStatus::DUAL_FEASIBLE;
    InterruptionCause cause ABSL_REQUIRE_EXPLICIT_INIT;
    friend bool operator==(const DualFeasible&, const DualFeasible&) = default;
  };
  struct Abnormal {
    static constexpr ProblemStatus status = ProblemStatus::ABNORMAL;
    AbnormalityCause cause ABSL_REQUIRE_EXPLICIT_INIT;
    friend bool operator==(const Abnormal&, const Abnormal&) = default;
  };
  struct InvalidProblem {
    static constexpr ProblemStatus status = ProblemStatus::INVALID_PROBLEM;
    friend bool operator==(const InvalidProblem&,
                           const InvalidProblem&) = default;
  };
  struct Imprecise {
    static constexpr ProblemStatus status = ProblemStatus::IMPRECISE;
    friend bool operator==(const Imprecise&, const Imprecise&) = default;
  };

  using Value = std::variant<Optimal, PrimalInfeasible, DualInfeasible,
                             InfeasibleOrUnbounded, PrimalUnbounded,
                             DualUnbounded, Init, PrimalFeasible, DualFeasible,
                             Abnormal, InvalidProblem, Imprecise>;

  // One of the XxxResult.
  //
  // To build a SolveStatus, see XxxSolveStatus() factories.
  Value value ABSL_REQUIRE_EXPLICIT_INIT;

  friend bool operator==(const SolveStatus&, const SolveStatus&) = default;

  // Returns true if `value` is the Alternative type.
  //
  // This is a shortcut for std::holds_alternative<Alternative>(status.value).
  //
  // Example:
  //   SolveStatus solve_status = ...;
  //   if (solve_status.Is<SolveStatus::Abnormal>()) {
  //     return solve_status;
  //   }
  template <typename Alternative>
  bool Is() const {
    return std::holds_alternative<Alternative>(value);
  }

  // Returns the equivalent ProblemStatus enum to the status.
  ProblemStatus problem_status() const;

  // Returns the interruption cause if there is one (depends on the
  // problem_status(), some status are not the result of an early interruption).
  std::optional<InterruptionCause> interruption_cause() const;

  // Returns a failing glop::Status iff this holds AbnormalResult.
  //
  // Used only in legacy API that returns a glop::Status.
  Status LegacyStatus() const;
};

// Returns the string representation of the SolveStatus.
std::string GetSolveStatusString(SolveStatus status);

// Prints the string representation of the SolveStatus.
std::ostream& operator<<(std::ostream& os, SolveStatus status);

std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::Optimal& alternative);
std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::PrimalInfeasible& alternative);
std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::DualInfeasible& alternative);
std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::InfeasibleOrUnbounded& alternative);
std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::PrimalUnbounded& alternative);
std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::DualUnbounded& alternative);
std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::Init& alternative);
std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::PrimalFeasible& alternative);
std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::DualFeasible& alternative);
std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::Abnormal& alternative);
std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::InvalidProblem& alternative);
std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::Imprecise& alternative);

// Returns a status with SolveStatus::Optimal alternative.
SolveStatus OptimalSolveStatus();

// Returns a status with SolveStatus::PrimalInfeasible alternative.
SolveStatus PrimalInfeasibleSolveStatus();

// Returns a status with SolveStatus::DualInfeasible alternative.
SolveStatus DualInfeasibleSolveStatus();

// Returns a status with SolveStatus::InfeasibleOrUnbounded alternative.
SolveStatus InfeasibleOrUnboundedSolveStatus();

// Returns a status with SolveStatus::PrimalUnbounded alternative.
SolveStatus PrimalUnboundedSolveStatus();

// Returns a status with SolveStatus::DualUnbounded alternative.
SolveStatus DualUnboundedSolveStatus();

// Returns a status with SolveStatus::Init alternative.
SolveStatus InitSolveStatus(InterruptionCause cause);

// Returns a status with SolveStatus::PrimalFeasible alternative.
//
// See FeasibleSolveStatus().
SolveStatus PrimalFeasibleSolveStatus(InterruptionCause cause);

// Returns a status with SolveStatus::DualFeasible alternative.
//
// See FeasibleSolveStatus().
SolveStatus DualFeasibleSolveStatus(InterruptionCause cause);

// Returns a status with either SolveStatus::PrimalFeasible or
// SolveStatus::DualFeasible alternative.
//
// This is used when a solve is interrupted to convert the temporary
// FeasibilityStatus into a final "only feasibility is known" status.
//
// See PrimalFeasibleSolveStatus() and DualFeasibleSolveStatus().
SolveStatus FeasibleSolveStatus(FeasibilityStatus feasibility,
                                InterruptionCause cause);

// Returns a status with SolveStatus::Abnormal alternative.
SolveStatus AbnormalSolveStatus(AbnormalityCause cause);

// Returns a status with SolveStatus::InvalidProblem alternative.
SolveStatus InvalidProblemSolveStatus();

// Returns a status with SolveStatus::Imprecise alternative.
SolveStatus ImpreciseSolveStatus();

// Different types of variables.
enum class VariableType : int8_t {
  UNCONSTRAINED,
  LOWER_BOUNDED,
  UPPER_BOUNDED,
  UPPER_AND_LOWER_BOUNDED,
  FIXED_VARIABLE
};

// Returns the string representation of the VariableType enum.
std::string GetVariableTypeString(VariableType variable_type);

inline std::ostream& operator<<(std::ostream& os, VariableType type) {
  os << GetVariableTypeString(type);
  return os;
}

// Different variables statuses.
// If a solution is OPTIMAL or FEASIBLE, then all the properties described here
// should be satisfied. These properties should also be true during the
// execution of the revised simplex algorithm, except that because of
// bound-shifting, the variable may not be at their exact bounds until the
// shifts are removed.
enum class VariableStatus : int8_t {
  // The basic status is special and takes precedence over all the other
  // statuses. It means that the variable is part of the basis.
  BASIC,
  // Only possible status of a FIXED_VARIABLE not in the basis. The variable
  // value should be exactly equal to its bounds (which are the same).
  FIXED_VALUE,
  // Only possible statuses of a non-basic variable which is not UNCONSTRAINED
  // or FIXED. The variable value should be at its exact specified bound (which
  // must be finite).
  AT_LOWER_BOUND,
  AT_UPPER_BOUND,
  // Only possible status of an UNCONSTRAINED non-basic variable.
  // Its value should be zero.
  //
  // Note that during crossover, this status is relaxed, and any variable that
  // can currently move in both directions can be marked as free.
  FREE,
};

// Returns the string representation of the VariableStatus enum.
std::string GetVariableStatusString(VariableStatus status);

inline std::ostream& operator<<(std::ostream& os, VariableStatus status) {
  os << GetVariableStatusString(status);
  return os;
}

// Different constraints statuses.
// The meaning is the same for the constraint activity relative to its bounds as
// it is for a variable value relative to its bounds. Actually, this is the
// VariableStatus of the slack variable associated to a constraint modulo a
// change of sign. The difference is that because of precision error, a
// constraint activity cannot exactly be equal to one of its bounds or to zero.
enum class ConstraintStatus : int8_t {
  BASIC,
  FIXED_VALUE,
  AT_LOWER_BOUND,
  AT_UPPER_BOUND,
  FREE,
};

// Returns the string representation of the ConstraintStatus enum.
std::string GetConstraintStatusString(ConstraintStatus status);

inline std::ostream& operator<<(std::ostream& os, ConstraintStatus status) {
  os << GetConstraintStatusString(status);
  return os;
}

// Returns the ConstraintStatus corresponding to a given VariableStatus.
ConstraintStatus VariableToConstraintStatus(VariableStatus status);

// A span of `T`, indexed by a strict int type `IntType`. Intended to be passed
// by value. See b/259677543.
template <typename IntType, typename T>
class StrictITISpan {
 public:
  using IndexType = IntType;
  using reference = T&;
  using value_type = T;

  StrictITISpan(T* data, IntType size) : data_(data), size_(size) {}

  reference operator[](IntType i) const {
    return data_[static_cast<size_t>(i.value())];
  }

  IntType size() const { return size_; }

  // TODO(user): This should probably be a strictly typed iterator too, but
  // `StrongVector::begin()` already suffers from this problem.
  auto begin() const { return data_; }
  auto end() const { return data_ + static_cast<size_t>(size_.value()); }

 private:
  T* const data_;
  const IntType size_;
};

// Wrapper around a StrongVector to allow (and enforce) creation/resize/assign
// to use the index type for the size.
//
// TODO(user): This should probably move to StrongVector, but note that this
// version is stricter and does not allow any other size types.
template <typename IntType, typename T, typename Alloc = std::allocator<T>>
class StrictITIVector : public util_intops::StrongVector<IntType, T, Alloc> {
 public:
  using IndexType = IntType;
  using ParentType = util_intops::StrongVector<IntType, T, Alloc>;
  using View = StrictITISpan<IntType, T>;
  using ConstView = StrictITISpan<IntType, const T>;

  StrictITIVector() = default;
  explicit StrictITIVector(IntType size) : ParentType(size) {}
  explicit StrictITIVector(const Alloc& a) : ParentType(a) {}
  StrictITIVector(IntType n, const T& v, const Alloc& a = Alloc())
      : ParentType(n, v, a) {}

  // This allows for brace initialization, which is really useful in tests.
  // It is not 'explicit' by design, so one can do vector = {...};
#if !defined(__ANDROID__)
  StrictITIVector(std::initializer_list<T> init_list,
                  const Alloc& a = Alloc())  // NOLINT
      : ParentType(init_list.begin(), init_list.end(), a) {}
#endif

  template <typename InputIteratorType>
  StrictITIVector(InputIteratorType first, InputIteratorType last,
                  const Alloc& a = Alloc())
      : ParentType(first, last, a) {}

  void resize(IntType size) { ParentType::resize(size.value()); }
  void resize(IntType size, const T& v) { ParentType::resize(size.value(), v); }

  void reserve(IntType size) { ParentType::reserve(size.value()); }

  void assign(IntType size, const T& v) { ParentType::assign(size.value(), v); }

  IntType size() const { return IntType(ParentType::size()); }

  IntType capacity() const { return IntType(ParentType::capacity()); }

  View view() { return View(ParentType::data(), size()); }
  ConstView const_view() const { return ConstView(ParentType::data(), size()); }
  ConstView view() const { return const_view(); }

  StrictITIVector& operator=(ConstView data) {
    ParentType::assign(data.begin(), data.end());
    return *this;
  }

  // Since calls to resize() must use a default value, we introduce a new
  // function for convenience to reduce the size of a vector.
  void resize_down(IntType size) {
    DCHECK_GE(size, IntType(0));
    DCHECK_LE(size, IntType(ParentType::size()));
    ParentType::resize(size.value());
  }

  // This function can be up to 4 times faster than calling assign(size, 0).
  // Note that it only works with StrictITIVector of basic types.
  void AssignToZero(IntType size) {
    resize(size, 0);
    memset(ParentType::data(), 0, size.value() * sizeof(T));
  }
};

// Row-vector types. Row-vector types are indexed by a column index.

// Row of fractional values.
typedef StrictITIVector<ColIndex, Fractional> DenseRow;

// Row of booleans.
typedef StrictITIVector<ColIndex, bool> DenseBooleanRow;

// Row of column indices. Used to represent mappings between columns.
typedef StrictITIVector<ColIndex, ColIndex> ColMapping;

// Vector of row or column indices. Useful to list the non-zero positions.
typedef std::vector<ColIndex> ColIndexVector;
typedef std::vector<RowIndex> RowIndexVector;

// Row of row indices.
// Useful for knowing which row corresponds to a particular column in the basis,
// or for storing the number of rows for a given column.
typedef StrictITIVector<ColIndex, RowIndex> ColToRowMapping;

// Row of variable types.
typedef StrictITIVector<ColIndex, VariableType> VariableTypeRow;

// Row of variable statuses.
typedef StrictITIVector<ColIndex, VariableStatus> VariableStatusRow;

// Row of bits.
typedef Bitset64<ColIndex> DenseBitRow;

// Column-vector types. Column-vector types are indexed by a row index.

// Column of fractional values.
typedef StrictITIVector<RowIndex, Fractional> DenseColumn;

// Column of booleans.
typedef StrictITIVector<RowIndex, bool> DenseBooleanColumn;

// Column of bits.
typedef Bitset64<RowIndex> DenseBitColumn;

// Column of row indices. Used to represent mappings between rows.
typedef StrictITIVector<RowIndex, RowIndex> RowMapping;

// Column of column indices.
// Used to represent which column corresponds to a particular row in the basis,
// or for storing the number of columns for a given row.
typedef StrictITIVector<RowIndex, ColIndex> RowToColMapping;

// Column of constraints (slack variables) statuses.
typedef StrictITIVector<RowIndex, ConstraintStatus> ConstraintStatusColumn;

// --------------------------------------------------------
// VectorIterator
// --------------------------------------------------------

// An iterator over the elements of a sparse data structure that stores the
// elements in arrays for indices and coefficients. The iterator is
// built as a wrapper over a sparse vector entry class; the concrete entry class
// is provided through the template argument EntryType.
template <typename EntryType>
class VectorIterator : EntryType {
 public:
  using Index = typename EntryType::Index;
  using Entry = EntryType;

  VectorIterator(const Index* indices, const Fractional* coefficients,
                 EntryIndex i)
      : EntryType(indices, coefficients, i) {}

  void operator++() { ++this->i_; }
  bool operator!=(const VectorIterator& other) const {
    // This operator is intended for use in natural range iteration ONLY.
    // Therefore, we prefer to use '<' so that a buggy range iteration which
    // start point is *after* its end point stops immediately, instead of
    // iterating 2^(number of bits of EntryIndex) times.
    return this->i_ < other.i_;
  }
  const Entry& operator*() const { return *this; }
};

// This is used during the deterministic time computation to convert a given
// number of floating-point operations to something in the same order of
// magnitude as a second (on a 2014 desktop).
static inline double DeterministicTimeForFpOperations(int64_t n) {
  const double kConversionFactor = 2e-9;
  return kConversionFactor * static_cast<double>(n);
}

namespace internal {

// Thin wrapper around AbnormalityCause that implicit converts to:
// * either AbnormalityStatus,
// * or SolveStatus (using AbnormalSolveStatus()).
//
// This allows using the same GLOP_RETURN_IF_ABNORMAL() macro for functions that
// returns either AbnormalityStatus or SolveStatus.
struct GlopReturnIfAbnormalError {
  operator AbnormalityStatus() const {  // NOLINT
    return AbnormalityStatus(cause);
  }
  operator SolveStatus() const {  // NOLINT
    return AbnormalSolveStatus(cause);
  }
  AbnormalityCause cause ABSL_REQUIRE_EXPLICIT_INIT;
};

}  // namespace internal
}  // namespace glop
}  // namespace operations_research

#endif  // ORTOOLS_LP_DATA_LP_TYPES_H_
