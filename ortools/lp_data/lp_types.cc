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

#include "ortools/lp_data/lp_types.h"

#include <concepts>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>

#include "absl/functional/overload.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "ortools/glop/status.h"

namespace operations_research {
namespace glop {

std::string GetProblemStatusString(ProblemStatus problem_status) {
  switch (problem_status) {
    case ProblemStatus::OPTIMAL:
      return "OPTIMAL";
    case ProblemStatus::PRIMAL_INFEASIBLE:
      return "PRIMAL_INFEASIBLE";
    case ProblemStatus::DUAL_INFEASIBLE:
      return "DUAL_INFEASIBLE";
    case ProblemStatus::INFEASIBLE_OR_UNBOUNDED:
      return "INFEASIBLE_OR_UNBOUNDED";
    case ProblemStatus::PRIMAL_UNBOUNDED:
      return "PRIMAL_UNBOUNDED";
    case ProblemStatus::DUAL_UNBOUNDED:
      return "DUAL_UNBOUNDED";
    case ProblemStatus::INIT:
      return "INIT";
    case ProblemStatus::PRIMAL_FEASIBLE:
      return "PRIMAL_FEASIBLE";
    case ProblemStatus::DUAL_FEASIBLE:
      return "DUAL_FEASIBLE";
    case ProblemStatus::ABNORMAL:
      return "ABNORMAL";
    case ProblemStatus::INVALID_PROBLEM:
      return "INVALID_PROBLEM";
    case ProblemStatus::IMPRECISE:
      return "IMPRECISE";
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid ProblemStatus " << static_cast<int>(problem_status);
  return "UNKNOWN ProblemStatus";
}

absl::string_view GetInterruptionCauseString(const InterruptionCause cause) {
  switch (cause) {
    case InterruptionCause::kTimeLimit:
      return "TIME_LIMIT";
    case InterruptionCause::kExternal:
      return "EXTERNAL";
    case InterruptionCause::kIterationLimit:
      return "ITERATION_LIMIT";
    case InterruptionCause::kObjectiveLimit:
      return "OBJECTIVE_LIMIT";
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid InterruptionCause " << static_cast<int>(cause);
  return "UNKNOWN InterruptionCause";
}

absl::string_view GetFeasibilityStatusString(
    const FeasibilityStatus feasibility) {
  switch (feasibility) {
    case FeasibilityStatus::kPrimal:
      return "primal";
    case FeasibilityStatus::kDual:
      return "dual";
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid FeasibilityStatus " << static_cast<int>(feasibility);
  return "UNKNOWN FeasibilityStatus";
}

absl::string_view GetAbnormalityCauseString(const AbnormalityCause cause) {
  // Macro that makes sure the returned string is the cause name without the `k`
  // prefix letter.
#define GLOP_ABNORMALITY_CAUSE_CASE(name) \
  case AbnormalityCause::k##name:         \
    return #name

  switch (cause) {
    GLOP_ABNORMALITY_CAUSE_CASE(LuFactorizationDegenerateRankOneUpdate);
    GLOP_ABNORMALITY_CAUSE_CASE(LuFactorizationNotSquare);
    GLOP_ABNORMALITY_CAUSE_CASE(LuFactorizationPivotTooSmall);
    GLOP_ABNORMALITY_CAUSE_CASE(LuFactorizationMarkowitzPivotTooSmall);
    GLOP_ABNORMALITY_CAUSE_CASE(LpSolverInconsistentSolution);
    GLOP_ABNORMALITY_CAUSE_CASE(RevisedSimplexUnboundedFeasibilityProblem);
    GLOP_ABNORMALITY_CAUSE_CASE(RevisedSimplexConditionNumberTooHigh);
    GLOP_ABNORMALITY_CAUSE_CASE(RevisedSimplexStepLengthOverflow);
    GLOP_ABNORMALITY_CAUSE_CASE(RevisedSimplexPivotTooSmall);
    GLOP_ABNORMALITY_CAUSE_CASE(SingletonPreprocessorNoUnmarkedEntry);
    GLOP_ABNORMALITY_CAUSE_CASE(DoubletonEqualityRowPreprocessorOverflow);
    // Should never be used; but better be complete (and obviously we want
    // this switch to be exhaustive).
    GLOP_ABNORMALITY_CAUSE_CASE(NotForUseWithExhaustiveSwitchStatements);
  }

#undef GLOP_ABNORMALITY_CAUSE_CASE

  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid AbnormalityCause " << static_cast<int>(cause);
  return "UNKNOWN AbnormalityCause";
}

std::ostream& operator<<(std::ostream& out, const AbnormalityStatus& status) {
  if (status.cause.has_value()) {
    out << "ABNORMALITY(" << *status.cause << ')';
  } else {
    out << "OK";
  }
  return out;
}

std::string GetSolveStatusString(const SolveStatus status) {
  std::ostringstream oss;
  oss << status;
  return oss.str();
}

namespace {

// Prints a SolveStatus::Xxx with a single data named `cause`
// (e.g. PrimalFeasibleResult or AbnormalResult).
//
// Statically asserts that Alternative only has `cause` and no other fields so
// that no new field can be forgotten.
template <typename Alternative>
void PrintSolveResultAlternativeWithCause(std::ostream& os,
                                          const Alternative& alternative) {
  static_assert(sizeof(Alternative) == sizeof(alternative.cause));
  os << GetProblemStatusString(Alternative::status)
     << "(cause: " << alternative.cause << ')';
}

// Prints a SolveStatus::Xxx with no data.
//
// Statically asserts that Alternative is empty so that no new field can be
// forgotten.
template <typename Alternative>
void PrintSolveResultEmptyAlternative(std::ostream& os,
                                      const Alternative& alternative) {
  // We can't use sizeof() here as it returns 1 for empty classes.
  static_assert(std::is_empty_v<std::remove_cvref_t<decltype(alternative)>>);
  os << GetProblemStatusString(Alternative::status);
}

}  // namespace

std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::Optimal& alternative) {
  PrintSolveResultEmptyAlternative(os, alternative);
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::PrimalInfeasible& alternative) {
  PrintSolveResultEmptyAlternative(os, alternative);
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::DualInfeasible& alternative) {
  PrintSolveResultEmptyAlternative(os, alternative);
  return os;
}

std::ostream& operator<<(
    std::ostream& os, const SolveStatus::InfeasibleOrUnbounded& alternative) {
  PrintSolveResultEmptyAlternative(os, alternative);
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::PrimalUnbounded& alternative) {
  PrintSolveResultEmptyAlternative(os, alternative);
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::DualUnbounded& alternative) {
  PrintSolveResultEmptyAlternative(os, alternative);
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::Init& alternative) {
  PrintSolveResultAlternativeWithCause(os, alternative);
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::PrimalFeasible& alternative) {
  PrintSolveResultAlternativeWithCause(os, alternative);
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::DualFeasible& alternative) {
  PrintSolveResultAlternativeWithCause(os, alternative);
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::Abnormal& alternative) {
  PrintSolveResultAlternativeWithCause(os, alternative);
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::InvalidProblem& alternative) {
  PrintSolveResultEmptyAlternative(os, alternative);
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const SolveStatus::Imprecise& alternative) {
  PrintSolveResultEmptyAlternative(os, alternative);
  return os;
}

std::ostream& operator<<(std::ostream& os, const SolveStatus status) {
  std::visit([&](const auto& alternative) { os << alternative; }, status.value);
  return os;
}

SolveStatus OptimalSolveStatus() { return {SolveStatus::Optimal{}}; }

SolveStatus PrimalInfeasibleSolveStatus() {
  return {SolveStatus::PrimalInfeasible{}};
}

SolveStatus DualInfeasibleSolveStatus() {
  return {SolveStatus::DualInfeasible{}};
}

SolveStatus InfeasibleOrUnboundedSolveStatus() {
  return {SolveStatus::InfeasibleOrUnbounded{}};
}

SolveStatus PrimalUnboundedSolveStatus() {
  return {SolveStatus::PrimalUnbounded{}};
}

SolveStatus DualUnboundedSolveStatus() {
  return {SolveStatus::DualUnbounded{}};
}

SolveStatus InitSolveStatus(InterruptionCause cause) {
  return {SolveStatus::Init{.cause = cause}};
}

SolveStatus PrimalFeasibleSolveStatus(InterruptionCause cause) {
  return {SolveStatus::PrimalFeasible{.cause = cause}};
}

SolveStatus DualFeasibleSolveStatus(InterruptionCause cause) {
  return {SolveStatus::DualFeasible{.cause = cause}};
}

SolveStatus FeasibleSolveStatus(const FeasibilityStatus feasibility,
                                const InterruptionCause cause) {
  switch (feasibility) {
    case FeasibilityStatus::kPrimal:
      return PrimalFeasibleSolveStatus(cause);
    case FeasibilityStatus::kDual:
      return DualFeasibleSolveStatus(cause);
  }
}

SolveStatus AbnormalSolveStatus(const AbnormalityCause cause) {
  return {SolveStatus::Abnormal{.cause = cause}};
}

SolveStatus InvalidProblemSolveStatus() {
  return {SolveStatus::InvalidProblem{}};
}

SolveStatus ImpreciseSolveStatus() { return {SolveStatus::Imprecise{}}; }

ProblemStatus SolveStatus::problem_status() const {
  return std::visit([](const auto& alternative) { return alternative.status; },
                    value);
}

std::optional<InterruptionCause> SolveStatus::interruption_cause() const {
  return std::visit(
      [](const auto& alternative) -> std::optional<InterruptionCause> {
        // Here we use C++20 `requires` to test if the alternative has a cause
        // field and its type is InterruptionCause.
        if constexpr (requires {
                        {
                          alternative.cause
                        } -> std::convertible_to<InterruptionCause>;
                      }) {
          return alternative.cause;
        } else {
          return std::nullopt;
        }
      },
      value);
}

Status SolveStatus::LegacyStatus() const {
  return std::visit(
      absl::Overload{
          [](const Abnormal& abnormal) {
            return Status(
                Status::ERROR_LU,
                std::string(GetAbnormalityCauseString(abnormal.cause)));
          },
          [](const auto&) { return Status::OK(); },
      },
      value);
}

std::string GetVariableTypeString(VariableType variable_type) {
  switch (variable_type) {
    case VariableType::UNCONSTRAINED:
      return "UNCONSTRAINED";
    case VariableType::LOWER_BOUNDED:
      return "LOWER_BOUNDED";
    case VariableType::UPPER_BOUNDED:
      return "UPPER_BOUNDED";
    case VariableType::UPPER_AND_LOWER_BOUNDED:
      return "UPPER_AND_LOWER_BOUNDED";
    case VariableType::FIXED_VARIABLE:
      return "FIXED_VARIABLE";
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid VariableType " << static_cast<int>(variable_type);
  return "UNKNOWN VariableType";
}

std::string GetVariableStatusString(VariableStatus status) {
  switch (status) {
    case VariableStatus::FREE:
      return "FREE";
    case VariableStatus::AT_LOWER_BOUND:
      return "AT_LOWER_BOUND";
    case VariableStatus::AT_UPPER_BOUND:
      return "AT_UPPER_BOUND";
    case VariableStatus::FIXED_VALUE:
      return "FIXED_VALUE";
    case VariableStatus::BASIC:
      return "BASIC";
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid VariableStatus " << static_cast<int>(status);
  return "UNKNOWN VariableStatus";
}

std::string GetConstraintStatusString(ConstraintStatus status) {
  switch (status) {
    case ConstraintStatus::FREE:
      return "FREE";
    case ConstraintStatus::AT_LOWER_BOUND:
      return "AT_LOWER_BOUND";
    case ConstraintStatus::AT_UPPER_BOUND:
      return "AT_UPPER_BOUND";
    case ConstraintStatus::FIXED_VALUE:
      return "FIXED_VALUE";
    case ConstraintStatus::BASIC:
      return "BASIC";
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid ConstraintStatus " << static_cast<int>(status);
  return "UNKNOWN ConstraintStatus";
}

ConstraintStatus VariableToConstraintStatus(VariableStatus status) {
  switch (status) {
    case VariableStatus::FREE:
      return ConstraintStatus::FREE;
    case VariableStatus::AT_LOWER_BOUND:
      return ConstraintStatus::AT_LOWER_BOUND;
    case VariableStatus::AT_UPPER_BOUND:
      return ConstraintStatus::AT_UPPER_BOUND;
    case VariableStatus::FIXED_VALUE:
      return ConstraintStatus::FIXED_VALUE;
    case VariableStatus::BASIC:
      return ConstraintStatus::BASIC;
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid VariableStatus " << static_cast<int>(status);
  // This will never be reached and is here only to guarantee compilation.
  return ConstraintStatus::FREE;
}

}  // namespace glop
}  // namespace operations_research
