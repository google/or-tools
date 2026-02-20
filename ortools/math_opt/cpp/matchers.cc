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

#include "ortools/math_opt/cpp/matchers.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/cpp/update_result.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"

namespace operations_research {
namespace math_opt {

namespace {

using ::testing::AllOf;
using ::testing::AllOfArray;
using ::testing::AnyOf;
using ::testing::AnyOfArray;
using ::testing::Contains;
using ::testing::DoubleNear;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::MatchResultListener;
using ::testing::Optional;
using ::testing::PrintToString;
using ::testing::Property;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Printing
////////////////////////////////////////////////////////////////////////////////

namespace {

template <typename T>
struct Printer {
  explicit Printer(const T& t) : value(t) {}

  const T& value;

  friend std::ostream& operator<<(std::ostream& os, const Printer& printer) {
    os << PrintToString(printer.value);
    return os;
  }
};

template <typename T>
Printer<T> Print(const T& t) {
  return Printer<T>(t);
}

}  // namespace

void PrintTo(const PrimalSolution& primal_solution, std::ostream* const os) {
  *os << "{variable_values: " << Print(primal_solution.variable_values)
      << ", objective_value: " << Print(primal_solution.objective_value)
      << ", feasibility_status: " << Print(primal_solution.feasibility_status)
      << "}";
}

void PrintTo(const DualSolution& dual_solution, std::ostream* const os) {
  *os << "{dual_values: " << Print(dual_solution.dual_values)
      << ", quadratic_dual_values: "
      << Print(dual_solution.quadratic_dual_values)
      << ", reduced_costs: " << Print(dual_solution.reduced_costs)
      << ", objective_value: " << Print(dual_solution.objective_value)
      << ", feasibility_status: " << Print(dual_solution.feasibility_status)
      << "}";
}

void PrintTo(const PrimalRay& primal_ray, std::ostream* const os) {
  *os << "{variable_values: " << Print(primal_ray.variable_values) << "}";
}

void PrintTo(const DualRay& dual_ray, std::ostream* const os) {
  *os << "{dual_values: " << Print(dual_ray.dual_values)
      << ", reduced_costs: " << Print(dual_ray.reduced_costs) << "}";
}

void PrintTo(const Basis& basis, std::ostream* const os) {
  *os << "{variable_status: " << Print(basis.variable_status)
      << ", constraint_status: " << Print(basis.constraint_status)
      << ", basic_dual_feasibility: " << Print(basis.basic_dual_feasibility)
      << "}";
}

void PrintTo(const Solution& solution, std::ostream* const os) {
  *os << "{primal_solution: " << Print(solution.primal_solution)
      << ", dual_solution: " << Print(solution.dual_solution)
      << ", basis: " << Print(solution.basis) << "}";
}

void PrintTo(const SolveResult& result, std::ostream* const os) {
  *os << "{termination: " << Print(result.termination)
      << ", solve_stats: " << Print(result.solve_stats)
      << ", solutions: " << Print(result.solutions)
      << ", primal_rays: " << Print(result.primal_rays)
      << ", dual_rays: " << Print(result.dual_rays) << "}";
}

////////////////////////////////////////////////////////////////////////////////
// absl::flat_hash_map<K, double> Matchers
////////////////////////////////////////////////////////////////////////////////

namespace {

template <typename K>
class MapToDoubleMatcher
    : public MatcherInterface<absl::flat_hash_map<K, double>> {
 public:
  MapToDoubleMatcher(absl::flat_hash_map<K, double> expected,
                     const bool all_keys, const double tolerance)
      : expected_(std::move(expected)),
        all_keys_(all_keys),
        tolerance_(tolerance) {
    for (const auto [k, v] : expected_) {
      CHECK(!std::isnan(v)) << "Illegal NaN for key: " << k;
    }
  }

  bool MatchAndExplain(absl::flat_hash_map<K, double> actual,
                       MatchResultListener* const os) const override {
    for (const auto& [key, value] : expected_) {
      if (!actual.contains(key)) {
        *os << "expected key " << key << " not found";
        return false;
      }
      if (!(std::abs(value - actual.at(key)) <= tolerance_)) {
        *os << "value for key " << key
            << " not within tolerance, expected: " << value
            << " but found: " << actual.at(key);
        return false;
      }
    }
    // Post condition: expected_ is a subset of actual.
    if (all_keys_ && expected_.size() != actual.size()) {
      for (const auto& [key, value] : actual) {
        if (!expected_.contains(key)) {
          *os << "found unexpected key " << key << " in actual";
          return false;
        }
      }
      // expected_ subset of actual && expected_.size() != actual.size() implies
      // that there is a member A of actual not in expected. When the loop above
      // hits A, it will return, thus this line is unreachable.
      LOG(FATAL) << "unreachable";
    }
    return true;
  }

  void DescribeTo(std::ostream* const os) const override {
    if (all_keys_) {
      *os << "has identical keys to ";
    } else {
      *os << "keys are contained in ";
    }
    PrintTo(expected_, os);
    *os << " and values within " << tolerance_;
  }

  void DescribeNegationTo(std::ostream* const os) const override {
    if (all_keys_) {
      *os << "either keys differ from ";
    } else {
      *os << "either has a key not in ";
    }
    PrintTo(expected_, os);
    *os << " or a value differs by more than " << tolerance_;
  }

 private:
  const absl::flat_hash_map<K, double> expected_;
  const bool all_keys_;
  const double tolerance_;
};

}  // namespace

Matcher<VariableMap<double>> IsNearlySupersetOf(VariableMap<double> expected,
                                                double tolerance) {
  return Matcher<VariableMap<double>>(new MapToDoubleMatcher<Variable>(
      std::move(expected), /*all_keys=*/false, tolerance));
}

Matcher<VariableMap<double>> IsNear(VariableMap<double> expected,
                                    const double tolerance) {
  return Matcher<VariableMap<double>>(new MapToDoubleMatcher<Variable>(
      std::move(expected), /*all_keys=*/true, tolerance));
}

Matcher<LinearConstraintMap<double>> IsNearlySupersetOf(
    LinearConstraintMap<double> expected, double tolerance) {
  return Matcher<LinearConstraintMap<double>>(
      new MapToDoubleMatcher<LinearConstraint>(std::move(expected),
                                               /*all_keys=*/false, tolerance));
}

Matcher<LinearConstraintMap<double>> IsNear(
    LinearConstraintMap<double> expected, const double tolerance) {
  return Matcher<LinearConstraintMap<double>>(
      new MapToDoubleMatcher<LinearConstraint>(std::move(expected),
                                               /*all_keys=*/true, tolerance));
}

Matcher<absl::flat_hash_map<QuadraticConstraint, double>> IsNear(
    absl::flat_hash_map<QuadraticConstraint, double> expected,
    const double tolerance) {
  return Matcher<absl::flat_hash_map<QuadraticConstraint, double>>(
      new MapToDoubleMatcher<QuadraticConstraint>(
          std::move(expected), /*all_keys=*/true, tolerance));
}

Matcher<absl::flat_hash_map<QuadraticConstraint, double>> IsNearlySupersetOf(
    absl::flat_hash_map<QuadraticConstraint, double> expected,
    double tolerance) {
  return Matcher<absl::flat_hash_map<QuadraticConstraint, double>>(
      new MapToDoubleMatcher<QuadraticConstraint>(std::move(expected),
                                                  /*all_keys=*/false,
                                                  tolerance));
}

template <typename K>
Matcher<absl::flat_hash_map<K, double>> IsNear(
    absl::flat_hash_map<K, double> expected, const double tolerance) {
  return Matcher<absl::flat_hash_map<K, double>>(new MapToDoubleMatcher<K>(
      std::move(expected), /*all_keys=*/true, tolerance));
}

template <typename K>
Matcher<absl::flat_hash_map<K, double>> IsNearlySupersetOf(
    absl::flat_hash_map<K, double> expected, const double tolerance) {
  return Matcher<absl::flat_hash_map<K, double>>(new MapToDoubleMatcher<K>(
      std::move(expected), /*all_keys=*/false, tolerance));
}

////////////////////////////////////////////////////////////////////////////////
// Matchers for LinearExpression and QuadraticExpression
////////////////////////////////////////////////////////////////////////////////

testing::Matcher<LinearExpression> IsIdentical(LinearExpression expected) {
  return LinearExpressionIsNear(expected, 0.0);
}

testing::Matcher<LinearExpression> LinearExpressionIsNear(
    const LinearExpression expected, const double tolerance) {
  CHECK(!std::isnan(expected.offset())) << "Illegal NaN-valued offset";
  return AllOf(
      Property("storage", &LinearExpression::storage, Eq(expected.storage())),
      Property("offset", &LinearExpression::offset,
               testing::DoubleNear(expected.offset(), tolerance)),
      Property("terms", &LinearExpression::terms,
               IsNear(expected.terms(), tolerance)));
}

namespace {
testing::Matcher<BoundedLinearExpression> IsNearForSign(
    const BoundedLinearExpression& expected, const double tolerance) {
  return AllOf(Property("upper_bound_minus_offset",
                        &BoundedLinearExpression::upper_bound_minus_offset,
                        testing::DoubleNear(expected.upper_bound_minus_offset(),
                                            tolerance)),
               Property("lower_bound_minus_offset",
                        &BoundedLinearExpression::lower_bound_minus_offset,
                        testing::DoubleNear(expected.lower_bound_minus_offset(),
                                            tolerance)),
               Field("expression", &BoundedLinearExpression::expression,
                     Property("terms", &LinearExpression::terms,
                              IsNear(expected.expression.terms(), tolerance))));
}
}  // namespace

testing::Matcher<BoundedLinearExpression> IsNearlyEquivalent(
    const BoundedLinearExpression& expected, const double tolerance) {
  const BoundedLinearExpression expected_negation(
      -expected.expression, /*lower_bound=*/-expected.upper_bound,
      /*upper_bound=*/-expected.lower_bound);
  return AnyOf(IsNearForSign(expected, tolerance),
               IsNearForSign(expected_negation, tolerance));
}

testing::Matcher<QuadraticExpression> IsIdentical(
    QuadraticExpression expected) {
  CHECK(!std::isnan(expected.offset())) << "Illegal NaN-valued offset";
  return AllOf(
      Property("storage", &QuadraticExpression::storage,
               Eq(expected.storage())),
      Property("offset", &QuadraticExpression::offset,
               testing::Eq(expected.offset())),
      Property("linear_terms", &QuadraticExpression::linear_terms,
               IsNear(expected.linear_terms(), /*tolerance=*/0)),
      Property("quadratic_terms", &QuadraticExpression::quadratic_terms,
               IsNear(expected.quadratic_terms(), /*tolerance=*/0)));
}

////////////////////////////////////////////////////////////////////////////////
// Matcher helpers
////////////////////////////////////////////////////////////////////////////////

namespace {

template <typename RayType>
class RayMatcher : public MatcherInterface<RayType> {
 public:
  RayMatcher(RayType expected, const double tolerance)
      : expected_(std::move(expected)), tolerance_(tolerance) {}
  void DescribeTo(std::ostream* os) const final {
    *os << "after L_inf normalization, is within tolerance: " << tolerance_
        << " of expected: ";
    PrintTo(expected_, os);
  }
  void DescribeNegationTo(std::ostream* const os) const final {
    *os << "after L_inf normalization, is not within tolerance: " << tolerance_
        << " of expected: ";
    PrintTo(expected_, os);
  }

 protected:
  const RayType expected_;
  const double tolerance_;
};

// Alias to use the std::optional templated adaptor.
Matcher<double> IsNear(double expected, const double tolerance) {
  return DoubleNear(expected, tolerance);
}

template <typename Type>
Matcher<std::optional<Type>> IsNear(std::optional<Type> expected,
                                    const double tolerance) {
  if (expected.has_value()) {
    return Optional(IsNear(*expected, tolerance));
  }
  return testing::Eq(std::nullopt);
}

template <typename Type>
Matcher<std::optional<Type>> IsNear(std::optional<Type> expected,
                                    const double tolerance,
                                    const bool allow_undetermined) {
  if (expected.has_value()) {
    return Optional(IsNear(*expected, tolerance, allow_undetermined));
  }
  return testing::Eq(std::nullopt);
}

// Custom std::optional for basis.
Matcher<std::optional<Basis>> BasisIs(const std::optional<Basis>& expected) {
  if (expected.has_value()) {
    return Optional(BasisIs(*expected));
  }
  return testing::Eq(std::nullopt);
}

testing::Matcher<std::vector<Solution>> IsNear(
    absl::Span<const Solution> expected_solutions,
    const SolutionMatcherOptions options) {
  if (expected_solutions.empty()) {
    return IsEmpty();
  }
  std::vector<Matcher<Solution>> matchers;
  for (const Solution& sol : expected_solutions) {
    matchers.push_back(IsNear(sol, options));
  }
  return ::testing::ElementsAreArray(matchers);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Matchers for Solutions
////////////////////////////////////////////////////////////////////////////////

Matcher<SolutionStatus> SolutionStatusIs(const SolutionStatus expected,
                                         const bool allow_undetermined) {
  if (allow_undetermined) {
    return AnyOf(Eq(expected), Eq(SolutionStatus::kUndetermined));
  }
  return Eq(expected);
}

Matcher<PrimalSolution> IsNear(PrimalSolution expected, const double tolerance,
                               const bool allow_undetermined) {
  return AllOf(
      Field("variable_values", &PrimalSolution::variable_values,
            IsNear(expected.variable_values, tolerance)),
      Field("objective_value", &PrimalSolution::objective_value,
            IsNear(expected.objective_value, tolerance)),
      Field("feasibility_status", &PrimalSolution::feasibility_status,
            SolutionStatusIs(expected.feasibility_status, allow_undetermined)));
}

Matcher<DualSolution> IsNear(DualSolution expected, const double tolerance,
                             const bool allow_undetermined) {
  return AllOf(
      Field("dual_values", &DualSolution::dual_values,
            IsNear(expected.dual_values, tolerance)),
      Field("quadratic_dual_values", &DualSolution::quadratic_dual_values,
            IsNear(expected.quadratic_dual_values, tolerance)),
      Field("reduced_costs", &DualSolution::reduced_costs,
            IsNear(expected.reduced_costs, tolerance)),
      Field("objective_value", &DualSolution::objective_value,
            IsNear(expected.objective_value, tolerance)),
      Field("feasibility_status", &DualSolution::feasibility_status,
            SolutionStatusIs(expected.feasibility_status, allow_undetermined)));
}

Matcher<Basis> BasisIs(const Basis& expected) {
  return AllOf(Field("variable_status", &Basis::variable_status,
                     expected.variable_status),
               Field("constraint_status", &Basis::constraint_status,
                     expected.constraint_status),
               Field("basic_dual_feasibility", &Basis::basic_dual_feasibility,
                     expected.basic_dual_feasibility));
}

Matcher<Solution> IsNear(Solution expected,
                         const SolutionMatcherOptions options) {
  std::vector<Matcher<Solution>> to_check;
  if (options.check_primal) {
    to_check.push_back(Field("primal_solution", &Solution::primal_solution,
                             IsNear(expected.primal_solution, options.tolerance,
                                    options.allow_undetermined)));
  }
  if (options.check_dual) {
    to_check.push_back(Field("dual_solution", &Solution::dual_solution,
                             IsNear(expected.dual_solution, options.tolerance,
                                    options.allow_undetermined)));
  }
  if (options.check_basis) {
    to_check.push_back(
        Field("basis", &Solution::basis, BasisIs(expected.basis)));
  }
  return AllOfArray(to_check);
}

////////////////////////////////////////////////////////////////////////////////
// Primal Ray Matcher
////////////////////////////////////////////////////////////////////////////////

namespace {

template <typename K>
double InfinityNorm(const absl::flat_hash_map<K, double>& vector) {
  double infinity_norm = 0.0;
  for (auto [id, value] : vector) {
    infinity_norm = std::max(infinity_norm, std::abs(value));
  }
  return infinity_norm;
}

// Returns a normalized primal ray.
//
// The normalization is done using infinity norm:
//
//   ray / ||ray||_inf
//
// If the input ray norm is zero, the ray is returned unchanged.
PrimalRay NormalizePrimalRay(PrimalRay ray) {
  const double norm = InfinityNorm(ray.variable_values);
  if (norm != 0.0) {
    for (auto& entry : ray.variable_values) {
      entry.second /= norm;
    }
  }
  return ray;
}

class PrimalRayMatcher : public RayMatcher<PrimalRay> {
 public:
  PrimalRayMatcher(PrimalRay expected, const double tolerance)
      : RayMatcher(std::move(expected), tolerance) {}

  bool MatchAndExplain(PrimalRay actual,
                       MatchResultListener* const os) const override {
    auto normalized_actual = NormalizePrimalRay(actual);
    auto normalized_expected = NormalizePrimalRay(expected_);
    if (os->IsInterested()) {
      *os << "actual normalized: " << PrintToString(normalized_actual)
          << ", expected normalized: " << PrintToString(normalized_expected);
    }
    return ExplainMatchResult(
        IsNear(normalized_expected.variable_values, tolerance_),
        normalized_actual.variable_values, os);
  }
};

}  // namespace

Matcher<PrimalRay> IsNear(PrimalRay expected, const double tolerance) {
  return Matcher<PrimalRay>(
      new PrimalRayMatcher(std::move(expected), tolerance));
}

Matcher<PrimalRay> PrimalRayIsNear(VariableMap<double> expected_var_values,
                                   const double tolerance) {
  PrimalRay expected;
  expected.variable_values = std::move(expected_var_values);
  return IsNear(expected, tolerance);
}

////////////////////////////////////////////////////////////////////////////////
// Dual Ray Matcher
////////////////////////////////////////////////////////////////////////////////

namespace {

// Returns a normalized dual ray.
//
// The normalization is done using infinity norm:
//
//   ray / ||ray||_inf
//
// If the input ray norm is zero, the ray is returned unchanged.
DualRay NormalizeDualRay(DualRay ray) {
  const double norm =
      std::max(InfinityNorm(ray.dual_values), InfinityNorm(ray.reduced_costs));
  if (norm != 0.0) {
    for (auto& entry : ray.dual_values) {
      entry.second /= norm;
    }
    for (auto& entry : ray.reduced_costs) {
      entry.second /= norm;
    }
  }
  return ray;
}

class DualRayMatcher : public RayMatcher<DualRay> {
 public:
  DualRayMatcher(DualRay expected, const double tolerance)
      : RayMatcher(std::move(expected), tolerance) {}

  bool MatchAndExplain(DualRay actual, MatchResultListener* os) const override {
    auto normalized_actual = NormalizeDualRay(actual);
    auto normalized_expected = NormalizeDualRay(expected_);
    if (os->IsInterested()) {
      *os << "actual normalized: " << PrintToString(normalized_actual)
          << ", expected normalized: " << PrintToString(normalized_expected);
    }
    return ExplainMatchResult(
               IsNear(normalized_expected.dual_values, tolerance_),
               normalized_actual.dual_values, os) &&
           ExplainMatchResult(
               IsNear(normalized_expected.reduced_costs, tolerance_),
               normalized_actual.reduced_costs, os);
  }
};

}  // namespace

Matcher<DualRay> IsNear(DualRay expected, const double tolerance) {
  return Matcher<DualRay>(new DualRayMatcher(std::move(expected), tolerance));
}

////////////////////////////////////////////////////////////////////////////////
// SolveResult termination reason matchers
////////////////////////////////////////////////////////////////////////////////

Matcher<ObjectiveBounds> ObjectiveBoundsNear(const ObjectiveBounds& expected,
                                             const double tolerance) {
  return AllOf(Field("primal_bound", &ObjectiveBounds::primal_bound,
                     DoubleNear(expected.primal_bound, tolerance)),
               Field("dual_bound", &ObjectiveBounds::dual_bound,
                     DoubleNear(expected.dual_bound, tolerance)));
}

Matcher<SolveResult> TerminatesWithOneOf(
    const std::vector<TerminationReason>& allowed) {
  return Field("termination", &SolveResult::termination,
               Field("reason", &Termination::reason, AnyOfArray(allowed)));
}

Matcher<SolveResult> TerminatesWith(const TerminationReason expected) {
  return Field("termination", &SolveResult::termination,
               Field("reason", &Termination::reason, expected));
}

namespace {

// Returns a matcher matching only Termination.limit.
//
// Note that this is different from LimitIs() which tests both the
// Termination.limit and the Termination.reason.
//
// It matches if either the limit is the expected one, or if it is kUndetermined
// and when allow_limit_undetermined is true.
testing::Matcher<SolveResult> TerminationLimitIs(
    const Limit expected, const bool allow_limit_undetermined) {
  if (allow_limit_undetermined) {
    return Field("termination", &SolveResult::termination,
                 Field("limit", &Termination::limit,
                       AnyOf(Limit::kUndetermined, expected)));
  }
  return Field("termination", &SolveResult::termination,
               Field("limit", &Termination::limit, expected));
}

}  // namespace

testing::Matcher<SolveResult> TerminatesWithLimit(
    const Limit expected, const bool allow_limit_undetermined) {
  std::vector<Matcher<SolveResult>> matchers;
  matchers.push_back(TerminationLimitIs(expected, allow_limit_undetermined));
  matchers.push_back(TerminatesWithOneOf(
      {TerminationReason::kFeasible, TerminationReason::kNoSolutionFound}));
  return AllOfArray(matchers);
}

testing::Matcher<SolveResult> TerminatesWithReasonFeasible(
    const Limit expected, const bool allow_limit_undetermined) {
  std::vector<Matcher<SolveResult>> matchers;
  matchers.push_back(TerminationLimitIs(expected, allow_limit_undetermined));
  matchers.push_back(TerminatesWith(TerminationReason::kFeasible));
  return AllOfArray(matchers);
}

testing::Matcher<SolveResult> TerminatesWithReasonNoSolutionFound(
    const Limit expected, const bool allow_limit_undetermined) {
  std::vector<Matcher<SolveResult>> matchers;
  matchers.push_back(TerminationLimitIs(expected, allow_limit_undetermined));
  matchers.push_back(TerminatesWith(TerminationReason::kNoSolutionFound));
  return AllOfArray(matchers);
}

template <typename MatcherType>
std::string MatcherToStringImpl(const MatcherType& matcher, const bool negate) {
  std::ostringstream os;
  if (negate) {
    matcher.DescribeNegationTo(&os);
  } else {
    matcher.DescribeTo(&os);
  }
  return os.str();
}

template <typename T>
std::string MatcherToString(const Matcher<T>& matcher, bool negate) {
  return MatcherToStringImpl(matcher, negate);
}

// clang-format off
// Polymorphic matchers do not always define DescribeTo,
// The <T> type may not be a matcher, but it will implement DescribeTo.
// clang-format on
template <typename T>
std::string MatcherToString(const ::testing::PolymorphicMatcher<T>& matcher,
                            bool negate) {
  return MatcherToStringImpl(matcher.impl(), negate);
}

MATCHER_P(FirstElementIs, first_element_matcher,
          (negation
               ? absl::StrCat("is empty or first element ",
                              MatcherToString(first_element_matcher, true))
               : absl::StrCat("has at least one element and first element ",
                              MatcherToString(first_element_matcher, false)))) {
  return ExplainMatchResult(UnorderedElementsAre(first_element_matcher),
                            absl::MakeSpan(arg).subspan(0, 1), result_listener);
}

Matcher<Termination> LimitIs(math_opt::Limit limit,
                             const Matcher<std::string> detail_matcher) {
  return AllOf(Field("reason", &Termination::reason,
                     AnyOf(TerminationReason::kFeasible,
                           TerminationReason::kNoSolutionFound)),
               Field("limit", &Termination::limit, limit),
               Field("detail", &Termination::detail, detail_matcher));
}

Matcher<Termination> ReasonIs(TerminationReason reason) {
  return Field("reason", &Termination::reason, reason);
}

Matcher<Termination> ReasonIsOptimal() {
  return ReasonIs(TerminationReason::kOptimal);
}

Matcher<ProblemStatus> ProblemStatusIs(const ProblemStatus& expected) {
  return AllOf(
      Field("primal_status", &ProblemStatus::primal_status,
            expected.primal_status),
      Field("dual_status", &ProblemStatus::dual_status, expected.dual_status),
      Field("primal_or_dual_infeasible",
            &ProblemStatus::primal_or_dual_infeasible,
            expected.primal_or_dual_infeasible));
}

Matcher<Termination> TerminationIsOptimal() {
  return AllOf(
      Field("reason", &Termination::reason, TerminationReason::kOptimal),
      Field("problem_status", &Termination::problem_status,
            ProblemStatusIs({.primal_status = FeasibilityStatus::kFeasible,
                             .dual_status = FeasibilityStatus::kFeasible,
                             .primal_or_dual_infeasible = false})));
}

Matcher<Termination> TerminationIsOptimal(
    const double primal_objective_value,
    const std::optional<double> dual_objective_value, const double tolerance) {
  return AllOf(
      TerminationIsOptimal(),
      Field("objective_bounds", &Termination::objective_bounds,
            ObjectiveBoundsNear({.primal_bound = primal_objective_value,
                                 .dual_bound = dual_objective_value.value_or(
                                     primal_objective_value)},
                                tolerance)));
}

Matcher<Termination> TerminationIsIgnoreDetail(const Termination& expected) {
  return AllOf(Field("reason", &Termination::reason, expected.reason),
               Field("limit", &Termination::limit, expected.limit));
}

Matcher<SolveResult> IsOptimal(
    const std::optional<double> expected_primal_objective,
    const double tolerance) {
  if (expected_primal_objective.has_value()) {
    return AllOf(
        Field("termination", &SolveResult::termination, TerminationIsOptimal()),
        Property("has_primal_feasible_solution",
                 &SolveResult::has_primal_feasible_solution, true),
        Property("objective_value", &SolveResult::objective_value,
                 DoubleNear(*expected_primal_objective, tolerance)));
  }
  return Field("termination", &SolveResult::termination,
               TerminationIsOptimal());
}

Matcher<SolveResult> IsOptimalWithSolution(
    const double expected_objective,
    const VariableMap<double> expected_variable_values,
    const double tolerance) {
  return AllOf(
      IsOptimal(std::make_optional(expected_objective), tolerance),
      HasSolution(
          PrimalSolution{.variable_values = expected_variable_values,
                         .objective_value = expected_objective,
                         .feasibility_status = SolutionStatus::kFeasible},
          tolerance));
}

Matcher<SolveResult> IsOptimalWithDualSolution(
    const double expected_objective,
    const LinearConstraintMap<double> expected_dual_values,
    const VariableMap<double> expected_reduced_costs, const double tolerance) {
  return AllOf(
      IsOptimal(std::make_optional(expected_objective), tolerance),
      HasDualSolution(
          DualSolution{
              .dual_values = expected_dual_values,
              .reduced_costs = expected_reduced_costs,
              .objective_value = std::make_optional(expected_objective),
              .feasibility_status = SolutionStatus::kFeasible},
          tolerance));
}

Matcher<SolveResult> IsOptimalWithDualSolution(
    const double expected_objective,
    const LinearConstraintMap<double> expected_dual_values,
    const absl::flat_hash_map<QuadraticConstraint, double>
        expected_quadratic_dual_values,
    const VariableMap<double> expected_reduced_costs, const double tolerance) {
  return AllOf(
      IsOptimal(std::make_optional(expected_objective), tolerance),
      HasDualSolution(
          DualSolution{
              .dual_values = expected_dual_values,
              .quadratic_dual_values = expected_quadratic_dual_values,
              .reduced_costs = expected_reduced_costs,
              .objective_value = std::make_optional(expected_objective),
              .feasibility_status = SolutionStatus::kFeasible},
          tolerance));
}

Matcher<SolveResult> HasSolution(PrimalSolution expected,
                                 const double tolerance) {
  return Field(
      "solutions", &SolveResult::solutions,
      Contains(Field("primal_solution", &Solution::primal_solution,
                     Optional(IsNear(std::move(expected), tolerance)))));
}

Matcher<SolveResult> HasDualSolution(DualSolution expected,
                                     const double tolerance) {
  return Field(
      "solutions", &SolveResult::solutions,
      Contains(Field("dual_solution", &Solution::dual_solution,
                     Optional(IsNear(std::move(expected), tolerance)))));
}

Matcher<SolveResult> HasPrimalRay(PrimalRay expected, const double tolerance) {
  return Field("primal_rays", &SolveResult::primal_rays,
               Contains(IsNear(std::move(expected), tolerance)));
}

Matcher<SolveResult> HasPrimalRay(VariableMap<double> expected_vars,
                                  const double tolerance) {
  PrimalRay ray;
  ray.variable_values = std::move(expected_vars);
  return HasPrimalRay(std::move(ray), tolerance);
}

Matcher<SolveResult> HasDualRay(DualRay expected, const double tolerance) {
  return Field("dual_rays", &SolveResult::dual_rays,
               Contains(IsNear(std::move(expected), tolerance)));
}

namespace {

bool MightTerminateWithRays(const TerminationReason reason) {
  switch (reason) {
    case TerminationReason::kInfeasibleOrUnbounded:
    case TerminationReason::kUnbounded:
    case TerminationReason::kInfeasible:
      return true;
    default:
      return false;
  }
}

std::vector<TerminationReason> CompatibleReasons(
    const TerminationReason expected, const bool inf_or_unb_soft_match) {
  if (!inf_or_unb_soft_match) {
    return {expected};
  }
  switch (expected) {
    case TerminationReason::kUnbounded:
      return {TerminationReason::kUnbounded,
              TerminationReason::kInfeasibleOrUnbounded};
    case TerminationReason::kInfeasible:
      return {TerminationReason::kInfeasible,
              TerminationReason::kInfeasibleOrUnbounded};
    case TerminationReason::kInfeasibleOrUnbounded:
      return {TerminationReason::kUnbounded, TerminationReason::kInfeasible,
              TerminationReason::kInfeasibleOrUnbounded};
    default:
      return {expected};
  }
}

Matcher<std::vector<Solution>> CheckSolutions(
    absl::Span<const Solution> expected_solutions,
    const SolveResultMatcherOptions& options) {
  if (options.first_solution_only && !expected_solutions.empty()) {
    return FirstElementIs(
        IsNear(expected_solutions[0],
               SolutionMatcherOptions{.tolerance = options.tolerance,
                                      .check_primal = true,
                                      .check_dual = options.check_dual,
                                      .check_basis = options.check_basis}));
  }
  return IsNear(expected_solutions,
                SolutionMatcherOptions{.tolerance = options.tolerance,
                                       .check_primal = true,
                                       .check_dual = options.check_dual,
                                       .check_basis = options.check_basis});
}

template <typename RayType>
Matcher<std::vector<RayType>> AnyRayNear(
    const std::vector<RayType>& expected_rays, const double tolerance) {
  std::vector<Matcher<RayType>> matchers;
  for (const RayType& ray : expected_rays) {
    matchers.push_back(IsNear(ray, tolerance));
  }
  return Contains(AnyOfArray(matchers));
}

template <typename RayType>
Matcher<std::vector<RayType>> AllRaysNear(
    const std::vector<RayType>& expected_rays, const double tolerance) {
  std::vector<Matcher<RayType>> matchers;
  for (const RayType& ray : expected_rays) {
    matchers.push_back(IsNear(ray, tolerance));
  }
  return ::testing::UnorderedElementsAreArray(matchers);
}

template <typename RayType>
Matcher<std::vector<RayType>> CheckRays(
    const std::vector<RayType>& expected_rays, const double tolerance,
    bool check_all) {
  if (expected_rays.empty()) {
    return IsEmpty();
  }
  if (check_all) {
    return AllRaysNear(expected_rays, tolerance);
  }
  return AnyRayNear(expected_rays, tolerance);
}

}  // namespace

Matcher<SolveResult> IsConsistentWith(
    const SolveResult& expected, const SolveResultMatcherOptions& options) {
  std::vector<Matcher<SolveResult>> to_check;
  to_check.push_back(TerminatesWithOneOf(CompatibleReasons(
      expected.termination.reason, options.inf_or_unb_soft_match)));
  const bool skip_solution =
      MightTerminateWithRays(expected.termination.reason) &&
      !options.check_solutions_if_inf_or_unbounded;
  if (!skip_solution) {
    to_check.push_back(Field("solutions", &SolveResult::solutions,
                             CheckSolutions(expected.solutions, options)));
  }
  if (options.check_rays) {
    to_check.push_back(Field("primal_rays", &SolveResult::primal_rays,
                             CheckRays(expected.primal_rays, options.tolerance,
                                       !options.first_solution_only)));
    to_check.push_back(Field("dual_rays", &SolveResult::dual_rays,
                             CheckRays(expected.dual_rays, options.tolerance,
                                       !options.first_solution_only)));
  }

  return AllOfArray(to_check);
}

////////////////////////////////////////////////////////////////////////////////
// ComputeInfeasibleSubsystemResult matchers
////////////////////////////////////////////////////////////////////////////////

testing::Matcher<ComputeInfeasibleSubsystemResult> IsFeasible() {
  return AllOf(
      Field("feasibility", &ComputeInfeasibleSubsystemResult::feasibility,
            FeasibilityStatus::kFeasible),
      Field("infeasible_subsystem",
            &ComputeInfeasibleSubsystemResult::infeasible_subsystem,
            Property("empty", &ModelSubset::empty, true)),
      Field("is_minimal", &ComputeInfeasibleSubsystemResult::is_minimal,
            false));
}

testing::Matcher<ComputeInfeasibleSubsystemResult> IsUndetermined() {
  return AllOf(
      Field("feasibility", &ComputeInfeasibleSubsystemResult::feasibility,
            FeasibilityStatus::kUndetermined),
      Field("infeasible_subsystem",
            &ComputeInfeasibleSubsystemResult::infeasible_subsystem,
            Property("empty", &ModelSubset::empty, true)),
      Field("is_minimal", &ComputeInfeasibleSubsystemResult::is_minimal,
            false));
}

testing::Matcher<ComputeInfeasibleSubsystemResult> IsInfeasible(
    const std::optional<bool> expected_is_minimal,
    const std::optional<ModelSubset> expected_infeasible_subsystem) {
  std::vector<Matcher<ComputeInfeasibleSubsystemResult>> matchers;
  matchers.push_back(Field("feasibility",
                           &ComputeInfeasibleSubsystemResult::feasibility,
                           FeasibilityStatus::kInfeasible));
  matchers.push_back(
      Field("infeasible_subsystem",
            &ComputeInfeasibleSubsystemResult::infeasible_subsystem,
            Property("empty", &ModelSubset::empty, false)));
  if (expected_is_minimal.has_value()) {
    matchers.push_back(Field("is_minimal",
                             &ComputeInfeasibleSubsystemResult::is_minimal,
                             Eq(expected_is_minimal.value())));
  }
  if (expected_infeasible_subsystem.has_value()) {
    matchers.push_back(
        Field("infeasible_subsystem",
              &ComputeInfeasibleSubsystemResult::infeasible_subsystem,
              Property(&ModelSubset::Proto,
                       testing::EqualsProto(
                           expected_infeasible_subsystem.value().Proto()))));
  }
  return AllOfArray(matchers);
}

////////////////////////////////////////////////////////////////////////////////
// Rarely used
////////////////////////////////////////////////////////////////////////////////

Matcher<UpdateResult> DidUpdate() {
  return Field("did_update", &UpdateResult::did_update, true);
}

}  // namespace math_opt
}  // namespace operations_research
